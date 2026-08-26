// CPU-backed Vulkan memory, buffer, and image objects.

#include "nv_log.h"
#include "nv_objects.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <new>

namespace {

constexpr VkDeviceSize kResourceAlignment = 256;
std::atomic<uint64_t> g_nextDeviceAddress{0x100000000ull};

nv::NullDevice* ToDevice(VkDevice handle) {
    return nv::ValidateHandle<nv::NullDevice>(handle, nv::kMagicDevice);
}

nv::NullDeviceMemory* ToMemory(VkDeviceMemory handle) {
    return nv::ValidateHandle<nv::NullDeviceMemory>(handle, nv::kMagicMemory);
}

nv::NullBuffer* ToBuffer(VkBuffer handle) {
    return nv::ValidateHandle<nv::NullBuffer>(handle, nv::kMagicBuffer);
}

nv::NullImage* ToImage(VkImage handle) {
    return nv::ValidateHandle<nv::NullImage>(handle, nv::kMagicImage);
}

VkDeviceSize SaturatingMultiply(VkDeviceSize left, VkDeviceSize right) {
    if (left == 0 || right == 0) return 0;
    const VkDeviceSize maximum = std::numeric_limits<VkDeviceSize>::max();
    return left > maximum / right ? maximum : left * right;
}

VkDeviceSize SaturatingAdd(VkDeviceSize left, VkDeviceSize right) {
    const VkDeviceSize maximum = std::numeric_limits<VkDeviceSize>::max();
    return left > maximum - right ? maximum : left + right;
}

VkDeviceSize AlignUp(VkDeviceSize size, VkDeviceSize alignment) {
    if (size > std::numeric_limits<VkDeviceSize>::max() - (alignment - 1)) {
        return std::numeric_limits<VkDeviceSize>::max();
    }
    return (size + alignment - 1) & ~(alignment - 1);
}

uint32_t MemoryTypeBits(const nv::NullDevice* device) {
    auto* pd = nv::ValidateHandle<nv::NullPhysicalDevice>(device->physDevice, nv::kMagicPhysicalDevice);
    if (!pd || pd->memProps.memoryTypeCount == 0) return 0;
    return pd->memProps.memoryTypeCount >= 32
        ? std::numeric_limits<uint32_t>::max()
        : (1u << pd->memProps.memoryTypeCount) - 1u;
}

VkMemoryRequirements BufferRequirements(const nv::NullBuffer* buffer) {
    VkMemoryRequirements requirements{};
    requirements.alignment = kResourceAlignment;
    requirements.size = AlignUp(std::max<VkDeviceSize>(buffer->size, 1), requirements.alignment);
    requirements.memoryTypeBits = MemoryTypeBits(buffer->device);
    return requirements;
}

VkMemoryRequirements BufferRequirements(nv::NullDevice* device, const VkBufferCreateInfo* info) {
    nv::NullBuffer temporary;
    temporary.device = device;
    temporary.size = info ? info->size : 0;
    return BufferRequirements(&temporary);
}

VkDeviceSize ImageStorageSize(const VkImageCreateInfo* info) {
    if (!info) return 0;

    // Sixteen bytes per texel intentionally overestimates common uncompressed and
    // block-compressed formats. Exact format packing is handled by copy semantics.
    VkDeviceSize total = 0;
    uint32_t width = info->extent.width;
    uint32_t height = info->extent.height;
    uint32_t depth = info->extent.depth;
    for (uint32_t mip = 0; mip < info->mipLevels; ++mip) {
        VkDeviceSize level = SaturatingMultiply(std::max(width, 1u), std::max(height, 1u));
        level = SaturatingMultiply(level, std::max(depth, 1u));
        level = SaturatingMultiply(level, std::max(info->arrayLayers, 1u));
        level = SaturatingMultiply(level, static_cast<VkDeviceSize>(info->samples));
        level = SaturatingMultiply(level, 16);
        total = SaturatingAdd(total, level);
        width = std::max(width >> 1, 1u);
        height = std::max(height >> 1, 1u);
        depth = std::max(depth >> 1, 1u);
    }
    return AlignUp(std::max<VkDeviceSize>(total, 1), kResourceAlignment);
}

VkMemoryRequirements ImageRequirements(const nv::NullImage* image) {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = image->imageType;
    info.format = image->format;
    info.extent = image->extent;
    info.mipLevels = image->mipLevels;
    info.arrayLayers = image->arrayLayers;
    info.samples = image->samples;

    VkMemoryRequirements requirements{};
    requirements.alignment = kResourceAlignment;
    requirements.size = ImageStorageSize(&info);
    requirements.memoryTypeBits = MemoryTypeBits(image->device);
    return requirements;
}

VkMemoryRequirements ImageRequirements(nv::NullDevice* device, const VkImageCreateInfo* info) {
    VkMemoryRequirements requirements{};
    requirements.alignment = kResourceAlignment;
    requirements.size = ImageStorageSize(info);
    requirements.memoryTypeBits = MemoryTypeBits(device);
    return requirements;
}

bool FitsAllocation(const VkMemoryRequirements& requirements,
                    const nv::NullDeviceMemory* memory,
                    VkDeviceSize offset) {
    return offset % requirements.alignment == 0 &&
           offset <= memory->allocationSize &&
           requirements.size <= memory->allocationSize - offset &&
           (requirements.memoryTypeBits & (1u << memory->memoryTypeIndex)) != 0;
}

bool ValidRange(const nv::NullDeviceMemory* memory, VkDeviceSize offset, VkDeviceSize size) {
    if (offset > memory->allocationSize) return false;
    if (size == VK_WHOLE_SIZE) return true;
    return size <= memory->allocationSize - offset;
}

} // namespace

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice device,
    const VkMemoryAllocateInfo* pAllocateInfo,
    const VkAllocationCallbacks*,
    VkDeviceMemory* pMemory) {
    auto* dev = ToDevice(device);
    if (!dev || !pAllocateInfo || !pMemory) return VK_ERROR_INITIALIZATION_FAILED;
    *pMemory = VK_NULL_HANDLE;

    auto* pd = nv::ValidateHandle<nv::NullPhysicalDevice>(dev->physDevice, nv::kMagicPhysicalDevice);
    if (!pd || pAllocateInfo->memoryTypeIndex >= pd->memProps.memoryTypeCount ||
        pAllocateInfo->allocationSize == 0 ||
        pAllocateInfo->allocationSize > static_cast<VkDeviceSize>(std::numeric_limits<size_t>::max())) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    auto* memory = new (std::nothrow) nv::NullDeviceMemory();
    if (!memory) return VK_ERROR_OUT_OF_HOST_MEMORY;
    memory->bytes.reset(new (std::nothrow) uint8_t[static_cast<size_t>(pAllocateInfo->allocationSize)]{});
    if (!memory->bytes) {
        delete memory;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    memory->device = dev;
    memory->allocationSize = pAllocateInfo->allocationSize;
    memory->memoryTypeIndex = pAllocateInfo->memoryTypeIndex;
    memory->handle = reinterpret_cast<VkDeviceMemory>(memory);
    *pMemory = memory->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkFreeMemory(
    VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* allocation = ToMemory(memory);
    if (!dev || !allocation || allocation->device != dev) return;
    allocation->magic = 0;
    delete allocation;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(
    VkDevice device,
    VkDeviceMemory memory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkMemoryMapFlags,
    void** ppData) {
    auto* dev = ToDevice(device);
    auto* allocation = ToMemory(memory);
    if (!dev || !allocation || allocation->device != dev || !ppData) {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    *ppData = nullptr;

    auto* pd = nv::ValidateHandle<nv::NullPhysicalDevice>(dev->physDevice, nv::kMagicPhysicalDevice);
    const VkMemoryPropertyFlags properties = pd->memProps.memoryTypes[allocation->memoryTypeIndex].propertyFlags;
    if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ||
        allocation->mapped || !ValidRange(allocation, offset, size)) {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    allocation->mapped = true;
    *ppData = allocation->bytes.get() + static_cast<size_t>(offset);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    auto* dev = ToDevice(device);
    auto* allocation = ToMemory(memory);
    if (dev && allocation && allocation->device == dev) allocation->mapped = false;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory2KHR(
    VkDevice device, const VkMemoryMapInfoKHR* pMemoryMapInfo, void** ppData) {
    if (!pMemoryMapInfo) return VK_ERROR_MEMORY_MAP_FAILED;
    return vkMapMemory(device, pMemoryMapInfo->memory, pMemoryMapInfo->offset,
                       pMemoryMapInfo->size, pMemoryMapInfo->flags, ppData);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkUnmapMemory2KHR(
    VkDevice device, const VkMemoryUnmapInfoKHR* pMemoryUnmapInfo) {
    if (!pMemoryUnmapInfo) return VK_ERROR_MEMORY_MAP_FAILED;
    auto* dev = ToDevice(device);
    auto* allocation = ToMemory(pMemoryUnmapInfo->memory);
    if (!dev || !allocation || allocation->device != dev) return VK_ERROR_MEMORY_MAP_FAILED;
    vkUnmapMemory(device, pMemoryUnmapInfo->memory);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(
    VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    auto* dev = ToDevice(device);
    if (!dev || (memoryRangeCount != 0 && !pMemoryRanges)) return VK_ERROR_MEMORY_MAP_FAILED;
    for (uint32_t i = 0; i < memoryRangeCount; ++i) {
        auto* memory = ToMemory(pMemoryRanges[i].memory);
        if (!memory || memory->device != dev ||
            !ValidRange(memory, pMemoryRanges[i].offset, pMemoryRanges[i].size)) {
            return VK_ERROR_MEMORY_MAP_FAILED;
        }
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(
    VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceMemoryCommitment(
    VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    auto* dev = ToDevice(device);
    auto* allocation = ToMemory(memory);
    if (pCommittedMemoryInBytes) {
        *pCommittedMemoryInBytes = dev && allocation && allocation->device == dev
            ? allocation->allocationSize : 0;
    }
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(
    VkDevice device,
    const VkBufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*,
    VkBuffer* pBuffer) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pBuffer || pCreateInfo->size == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pBuffer = VK_NULL_HANDLE;
    auto* buffer = new (std::nothrow) nv::NullBuffer();
    if (!buffer) return VK_ERROR_OUT_OF_HOST_MEMORY;
    buffer->device = dev;
    buffer->size = pCreateInfo->size;
    buffer->usage = pCreateInfo->usage;
    buffer->flags = pCreateInfo->flags;
    buffer->fakeDeviceAddress = g_nextDeviceAddress.fetch_add(AlignUp(buffer->size, 4096));
    buffer->handle = reinterpret_cast<VkBuffer>(buffer);
    *pBuffer = buffer->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(
    VkDevice device, VkBuffer buffer, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = ToBuffer(buffer);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    auto* dev = ToDevice(device);
    auto* object = ToBuffer(buffer);
    if (!dev || !object || object->device != dev || !pMemoryRequirements) return;
    *pMemoryRequirements = BufferRequirements(object);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2(
    VkDevice device,
    const VkBufferMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    if (!pInfo || !pMemoryRequirements) return;
    vkGetBufferMemoryRequirements(device, pInfo->buffer, &pMemoryRequirements->memoryRequirements);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2KHR(
    VkDevice device,
    const VkBufferMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceBufferMemoryRequirements(
    VkDevice device,
    const VkDeviceBufferMemoryRequirements* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    auto* dev = ToDevice(device);
    if (!dev || !pInfo || !pInfo->pCreateInfo || !pMemoryRequirements) return;
    pMemoryRequirements->memoryRequirements = BufferRequirements(dev, pInfo->pCreateInfo);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceBufferMemoryRequirementsKHR(
    VkDevice device,
    const VkDeviceBufferMemoryRequirements* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    vkGetDeviceBufferMemoryRequirements(device, pInfo, pMemoryRequirements);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(
    VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    auto* dev = ToDevice(device);
    auto* object = ToBuffer(buffer);
    auto* allocation = ToMemory(memory);
    if (!dev || !object || !allocation || object->device != dev || allocation->device != dev ||
        !FitsAllocation(BufferRequirements(object), allocation, memoryOffset)) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    object->memory = allocation;
    object->memoryOffset = memoryOffset;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory2(
    VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    if (bindInfoCount != 0 && !pBindInfos) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < bindInfoCount; ++i) {
        VkResult result = vkBindBufferMemory(device, pBindInfos[i].buffer,
                                             pBindInfos[i].memory, pBindInfos[i].memoryOffset);
        if (result != VK_SUCCESS) return result;
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory2KHR(
    VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    return vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
}

extern "C" VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddress(
    VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    auto* dev = ToDevice(device);
    auto* buffer = pInfo ? ToBuffer(pInfo->buffer) : nullptr;
    return dev && buffer && buffer->device == dev ? buffer->fakeDeviceAddress : 0;
}

extern "C" VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddressKHR(
    VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    return vkGetBufferDeviceAddress(device, pInfo);
}

extern "C" VKAPI_ATTR uint64_t VKAPI_CALL vkGetBufferOpaqueCaptureAddress(
    VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    return vkGetBufferDeviceAddress(device, pInfo);
}

extern "C" VKAPI_ATTR uint64_t VKAPI_CALL vkGetBufferOpaqueCaptureAddressKHR(
    VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    return vkGetBufferOpaqueCaptureAddress(device, pInfo);
}

extern "C" VKAPI_ATTR uint64_t VKAPI_CALL vkGetDeviceMemoryOpaqueCaptureAddress(
    VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    auto* dev = ToDevice(device);
    auto* memory = pInfo ? ToMemory(pInfo->memory) : nullptr;
    return dev && memory && memory->device == dev
        ? reinterpret_cast<uint64_t>(memory->bytes.get()) : 0;
}

extern "C" VKAPI_ATTR uint64_t VKAPI_CALL vkGetDeviceMemoryOpaqueCaptureAddressKHR(
    VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    return vkGetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(
    VkDevice device,
    const VkImageCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*,
    VkImage* pImage) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pImage || pCreateInfo->extent.width == 0 ||
        pCreateInfo->extent.height == 0 || pCreateInfo->extent.depth == 0 ||
        pCreateInfo->mipLevels == 0 || pCreateInfo->arrayLayers == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pImage = VK_NULL_HANDLE;
    auto* image = new (std::nothrow) nv::NullImage();
    if (!image) return VK_ERROR_OUT_OF_HOST_MEMORY;
    image->device = dev;
    image->imageType = pCreateInfo->imageType;
    image->format = pCreateInfo->format;
    image->extent = pCreateInfo->extent;
    image->mipLevels = pCreateInfo->mipLevels;
    image->arrayLayers = pCreateInfo->arrayLayers;
    image->samples = pCreateInfo->samples;
    image->tiling = pCreateInfo->tiling;
    image->usage = pCreateInfo->usage;
    image->flags = pCreateInfo->flags;
    image->currentLayout = pCreateInfo->initialLayout;
    image->handle = reinterpret_cast<VkImage>(image);
    *pImage = image->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyImage(
    VkDevice device, VkImage image, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = ToImage(image);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    auto* dev = ToDevice(device);
    auto* object = ToImage(image);
    if (!dev || !object || object->device != dev || !pMemoryRequirements) return;
    *pMemoryRequirements = ImageRequirements(object);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2(
    VkDevice device,
    const VkImageMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    if (!pInfo || !pMemoryRequirements) return;
    vkGetImageMemoryRequirements(device, pInfo->image, &pMemoryRequirements->memoryRequirements);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2KHR(
    VkDevice device,
    const VkImageMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageMemoryRequirements(
    VkDevice device,
    const VkDeviceImageMemoryRequirements* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    auto* dev = ToDevice(device);
    if (!dev || !pInfo || !pInfo->pCreateInfo || !pMemoryRequirements) return;
    pMemoryRequirements->memoryRequirements = ImageRequirements(dev, pInfo->pCreateInfo);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageMemoryRequirementsKHR(
    VkDevice device,
    const VkDeviceImageMemoryRequirements* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
    vkGetDeviceImageMemoryRequirements(device, pInfo, pMemoryRequirements);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(
    VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    auto* dev = ToDevice(device);
    auto* object = ToImage(image);
    auto* allocation = ToMemory(memory);
    if (!dev || !object || !allocation || object->device != dev || allocation->device != dev ||
        !FitsAllocation(ImageRequirements(object), allocation, memoryOffset)) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    object->memory = allocation;
    object->memoryOffset = memoryOffset;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory2(
    VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    if (bindInfoCount != 0 && !pBindInfos) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < bindInfoCount; ++i) {
        VkResult result = vkBindImageMemory(device, pBindInfos[i].image,
                                            pBindInfos[i].memory, pBindInfos[i].memoryOffset);
        if (result != VK_SUCCESS) return result;
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory2KHR(
    VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    return vkBindImageMemory2(device, bindInfoCount, pBindInfos);
}
