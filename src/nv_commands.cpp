// Command recording and immediate CPU execution for resource operations.

#include "nv_log.h"
#include "nv_objects.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace {

struct FormatInfo {
    uint32_t blockWidth = 1;
    uint32_t blockHeight = 1;
    uint32_t blockBytes = 4;
};

nv::NullDevice* ToDevice(VkDevice handle) {
    return nv::ValidateHandle<nv::NullDevice>(handle, nv::kMagicDevice);
}

nv::NullQueue* ToQueue(VkQueue handle) {
    return nv::ValidateHandle<nv::NullQueue>(handle, nv::kMagicQueue);
}

nv::NullCommandPool* ToCommandPool(VkCommandPool handle) {
    return nv::ValidateHandle<nv::NullCommandPool>(handle, nv::kMagicCommandPool);
}

nv::NullCommandBuffer* ToCommandBuffer(VkCommandBuffer handle) {
    return nv::ValidateHandle<nv::NullCommandBuffer>(handle, nv::kMagicCommandBuffer);
}

nv::NullBuffer* ToBuffer(VkBuffer handle) {
    return nv::ValidateHandle<nv::NullBuffer>(handle, nv::kMagicBuffer);
}

nv::NullImage* ToImage(VkImage handle) {
    return nv::ValidateHandle<nv::NullImage>(handle, nv::kMagicImage);
}

uint32_t DivideRoundUp(uint32_t value, uint32_t divisor) {
    return value / divisor + (value % divisor != 0 ? 1u : 0u);
}

bool CheckedAdd(VkDeviceSize left, VkDeviceSize right, VkDeviceSize* result) {
    if (left > std::numeric_limits<VkDeviceSize>::max() - right) return false;
    *result = left + right;
    return true;
}

FormatInfo GetFormatInfo(VkFormat format) {
    const int value = static_cast<int>(format);
    if (format == VK_FORMAT_R4G4_UNORM_PACK8) return {1, 1, 1};
    if (value >= VK_FORMAT_R4G4B4A4_UNORM_PACK16 && value <= VK_FORMAT_A1R5G5B5_UNORM_PACK16) return {1, 1, 2};
    if (value >= VK_FORMAT_R8_UNORM && value <= VK_FORMAT_R8_SRGB) return {1, 1, 1};
    if (value >= VK_FORMAT_R8G8_UNORM && value <= VK_FORMAT_R8G8_SRGB) return {1, 1, 2};
    if (value >= VK_FORMAT_R8G8B8_UNORM && value <= VK_FORMAT_B8G8R8_SRGB) return {1, 1, 3};
    if (value >= VK_FORMAT_R8G8B8A8_UNORM && value <= VK_FORMAT_A8B8G8R8_SRGB_PACK32) return {1, 1, 4};
    if (value >= VK_FORMAT_A2R10G10B10_UNORM_PACK32 && value <= VK_FORMAT_E5B9G9R9_UFLOAT_PACK32) return {1, 1, 4};
    if (value >= VK_FORMAT_R16_UNORM && value <= VK_FORMAT_R16_SFLOAT) return {1, 1, 2};
    if (value >= VK_FORMAT_R16G16_UNORM && value <= VK_FORMAT_R16G16_SFLOAT) return {1, 1, 4};
    if (value >= VK_FORMAT_R16G16B16_UNORM && value <= VK_FORMAT_R16G16B16_SFLOAT) return {1, 1, 6};
    if (value >= VK_FORMAT_R16G16B16A16_UNORM && value <= VK_FORMAT_R16G16B16A16_SFLOAT) return {1, 1, 8};
    if (value >= VK_FORMAT_R32_UINT && value <= VK_FORMAT_R32_SFLOAT) return {1, 1, 4};
    if (value >= VK_FORMAT_R32G32_UINT && value <= VK_FORMAT_R32G32_SFLOAT) return {1, 1, 8};
    if (value >= VK_FORMAT_R32G32B32_UINT && value <= VK_FORMAT_R32G32B32_SFLOAT) return {1, 1, 12};
    if (value >= VK_FORMAT_R32G32B32A32_UINT && value <= VK_FORMAT_R32G32B32A32_SFLOAT) return {1, 1, 16};
    if (value >= VK_FORMAT_R64_UINT && value <= VK_FORMAT_R64_SFLOAT) return {1, 1, 8};
    if (value >= VK_FORMAT_R64G64_UINT && value <= VK_FORMAT_R64G64_SFLOAT) return {1, 1, 16};
    if (value >= VK_FORMAT_R64G64B64_UINT && value <= VK_FORMAT_R64G64B64_SFLOAT) return {1, 1, 24};
    if (value >= VK_FORMAT_R64G64B64A64_UINT && value <= VK_FORMAT_R64G64B64A64_SFLOAT) return {1, 1, 32};

    switch (format) {
    case VK_FORMAT_D16_UNORM: return {1, 1, 2};
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D24_UNORM_S8_UINT: return {1, 1, 4};
    case VK_FORMAT_S8_UINT: return {1, 1, 1};
    case VK_FORMAT_D16_UNORM_S8_UINT: return {1, 1, 3};
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return {1, 1, 8};
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK: return {4, 4, 8};
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK: return {4, 4, 16};
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK: return {4, 4, 8};
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return {4, 4, 16};
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return {4, 4, 16};
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return {5, 4, 16};
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return {5, 5, 16};
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return {6, 5, 16};
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return {6, 6, 16};
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return {8, 5, 16};
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return {8, 6, 16};
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return {8, 8, 16};
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return {10, 5, 16};
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return {10, 6, 16};
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return {10, 8, 16};
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return {10, 10, 16};
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return {12, 10, 16};
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return {12, 12, 16};
    default: return {1, 1, 4};
    }
}

VkExtent3D MipExtent(const nv::NullImage* image, uint32_t mipLevel) {
    return {
        std::max(image->extent.width >> mipLevel, 1u),
        std::max(image->extent.height >> mipLevel, 1u),
        std::max(image->extent.depth >> mipLevel, 1u),
    };
}

VkDeviceSize MipSize(const nv::NullImage* image, uint32_t mipLevel) {
    const FormatInfo format = GetFormatInfo(image->format);
    const VkExtent3D extent = MipExtent(image, mipLevel);
    const VkDeviceSize rowPitch = static_cast<VkDeviceSize>(DivideRoundUp(extent.width, format.blockWidth)) *
                                  format.blockBytes * static_cast<uint32_t>(image->samples);
    return rowPitch * DivideRoundUp(extent.height, format.blockHeight) * extent.depth;
}

VkDeviceSize SubresourceOffset(const nv::NullImage* image, uint32_t mipLevel, uint32_t arrayLayer) {
    VkDeviceSize layerSize = 0;
    for (uint32_t mip = 0; mip < image->mipLevels; ++mip) layerSize += MipSize(image, mip);
    VkDeviceSize offset = layerSize * arrayLayer;
    for (uint32_t mip = 0; mip < mipLevel; ++mip) offset += MipSize(image, mip);
    return offset;
}

bool BufferPointer(nv::NullBuffer* buffer, VkDeviceSize offset, VkDeviceSize size, uint8_t** pointer) {
    if (!buffer || !buffer->memory || !pointer || offset > buffer->size || size > buffer->size - offset) {
        return false;
    }
    VkDeviceSize absolute = 0;
    if (!CheckedAdd(buffer->memoryOffset, offset, &absolute) ||
        absolute > buffer->memory->allocationSize || size > buffer->memory->allocationSize - absolute) {
        return false;
    }
    *pointer = buffer->memory->bytes.get() + static_cast<size_t>(absolute);
    return true;
}

bool ImagePointer(nv::NullImage* image, VkDeviceSize offset, VkDeviceSize size, uint8_t** pointer) {
    if (!image || !image->memory || !pointer) return false;
    VkDeviceSize absolute = 0;
    if (!CheckedAdd(image->memoryOffset, offset, &absolute) ||
        absolute > image->memory->allocationSize || size > image->memory->allocationSize - absolute) {
        return false;
    }
    *pointer = image->memory->bytes.get() + static_cast<size_t>(absolute);
    return true;
}

VkDeviceSize AvailableBufferBytes(const nv::NullBuffer* buffer, VkDeviceSize offset) {
    if (!buffer || !buffer->memory || offset >= buffer->size) return 0;
    VkDeviceSize absolute = 0;
    if (!CheckedAdd(buffer->memoryOffset, offset, &absolute) ||
        absolute >= buffer->memory->allocationSize) return 0;
    return std::min(buffer->size - offset, buffer->memory->allocationSize - absolute);
}

bool CopyBuffer(nv::NullBuffer* source, nv::NullBuffer* destination,
                const std::vector<VkBufferCopy>& regions) {
    for (const auto& region : regions) {
        uint8_t* src = nullptr;
        uint8_t* dst = nullptr;
        if (!BufferPointer(source, region.srcOffset, region.size, &src) ||
            !BufferPointer(destination, region.dstOffset, region.size, &dst)) {
            NV_ERR("CopyBuffer range rejected: srcOffset=%llu dstOffset=%llu size=%llu srcSize=%llu dstSize=%llu",
                   region.srcOffset, region.dstOffset, region.size,
                   source ? source->size : 0, destination ? destination->size : 0);
            return false;
        }
        std::memmove(dst, src, static_cast<size_t>(region.size));
    }
    return true;
}

bool CopyBufferImageRegion(nv::NullBuffer* buffer, nv::NullImage* image,
                           const VkBufferImageCopy& region, bool bufferToImage) {
    if (!buffer || !image || region.imageSubresource.mipLevel >= image->mipLevels ||
        region.imageSubresource.baseArrayLayer >= image->arrayLayers ||
        region.imageSubresource.layerCount > image->arrayLayers - region.imageSubresource.baseArrayLayer ||
        region.imageOffset.x < 0 || region.imageOffset.y < 0 || region.imageOffset.z < 0) {
        NV_ERR("CopyBufferImage invalid subresource: image=%p mip=%u/%u layer=%u+%u/%u",
               image, region.imageSubresource.mipLevel, image ? image->mipLevels : 0,
               region.imageSubresource.baseArrayLayer, region.imageSubresource.layerCount,
               image ? image->arrayLayers : 0);
        return false;
    }

    const FormatInfo format = GetFormatInfo(image->format);
    const VkExtent3D mipExtent = MipExtent(image, region.imageSubresource.mipLevel);
    const uint32_t x = static_cast<uint32_t>(region.imageOffset.x);
    const uint32_t y = static_cast<uint32_t>(region.imageOffset.y);
    const uint32_t z = static_cast<uint32_t>(region.imageOffset.z);
    if (region.imageExtent.width > mipExtent.width - std::min(x, mipExtent.width) ||
        region.imageExtent.height > mipExtent.height - std::min(y, mipExtent.height) ||
        region.imageExtent.depth > mipExtent.depth - std::min(z, mipExtent.depth) ||
        x >= mipExtent.width || y >= mipExtent.height || z >= mipExtent.depth) {
        NV_ERR("CopyBufferImage invalid extent: format=%d mipExtent=%ux%ux%u offset=%d,%d,%d extent=%ux%ux%u",
               image->format, mipExtent.width, mipExtent.height, mipExtent.depth,
               region.imageOffset.x, region.imageOffset.y, region.imageOffset.z,
               region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth);
        return false;
    }

    const uint32_t rowTexels = region.bufferRowLength ? region.bufferRowLength : region.imageExtent.width;
    const uint32_t imageRows = region.bufferImageHeight ? region.bufferImageHeight : region.imageExtent.height;
    if (rowTexels < region.imageExtent.width || imageRows < region.imageExtent.height) {
        NV_ERR("CopyBufferImage invalid pitch: rowTexels=%u imageRows=%u extent=%ux%u",
               rowTexels, imageRows, region.imageExtent.width, region.imageExtent.height);
        return false;
    }

    const VkDeviceSize bufferRowPitch = static_cast<VkDeviceSize>(DivideRoundUp(rowTexels, format.blockWidth)) *
                                        format.blockBytes;
    const VkDeviceSize bufferSlicePitch = bufferRowPitch * DivideRoundUp(imageRows, format.blockHeight);
    const VkDeviceSize bufferLayerPitch = bufferSlicePitch * region.imageExtent.depth;
    const VkDeviceSize imageRowPitch = static_cast<VkDeviceSize>(DivideRoundUp(mipExtent.width, format.blockWidth)) *
                                       format.blockBytes * static_cast<uint32_t>(image->samples);
    const VkDeviceSize imageSlicePitch = imageRowPitch * DivideRoundUp(mipExtent.height, format.blockHeight);
    const VkDeviceSize rowBytes = static_cast<VkDeviceSize>(DivideRoundUp(region.imageExtent.width, format.blockWidth)) *
                                  format.blockBytes;
    const uint32_t rows = DivideRoundUp(region.imageExtent.height, format.blockHeight);

    for (uint32_t layer = 0; layer < region.imageSubresource.layerCount; ++layer) {
        const VkDeviceSize subresource = SubresourceOffset(
            image, region.imageSubresource.mipLevel, region.imageSubresource.baseArrayLayer + layer);
        for (uint32_t depth = 0; depth < region.imageExtent.depth; ++depth) {
            for (uint32_t row = 0; row < rows; ++row) {
                const VkDeviceSize bufferOffset = region.bufferOffset + layer * bufferLayerPitch +
                                                  depth * bufferSlicePitch + row * bufferRowPitch;
                const VkDeviceSize imageOffset = subresource + (z + depth) * imageSlicePitch +
                                                 (y / format.blockHeight + row) * imageRowPitch +
                                                 (x / format.blockWidth) * format.blockBytes;
                uint8_t* bufferBytes = nullptr;
                uint8_t* imageBytes = nullptr;
                if (!ImagePointer(image, imageOffset, rowBytes, &imageBytes)) return false;

                const VkDeviceSize available = std::min(rowBytes, AvailableBufferBytes(buffer, bufferOffset));
                if (available != 0 &&
                    !BufferPointer(buffer, bufferOffset, available, &bufferBytes)) return false;
                if (bufferToImage) {
                    if (available != 0) {
                        std::memcpy(imageBytes, bufferBytes, static_cast<size_t>(available));
                    }
                    if (available < rowBytes) {
                        std::memset(imageBytes + static_cast<size_t>(available), 0,
                                    static_cast<size_t>(rowBytes - available));
                    }
                } else if (available != 0) {
                    std::memcpy(bufferBytes, imageBytes, static_cast<size_t>(available));
                }
            }
        }
    }
    return true;
}

bool CopyImageRegion(nv::NullImage* source, nv::NullImage* destination, const VkImageCopy& region) {
    if (!source || !destination || region.srcSubresource.mipLevel >= source->mipLevels ||
        region.dstSubresource.mipLevel >= destination->mipLevels ||
        region.srcSubresource.layerCount != region.dstSubresource.layerCount ||
        region.srcOffset.x < 0 || region.srcOffset.y < 0 || region.srcOffset.z < 0 ||
        region.dstOffset.x < 0 || region.dstOffset.y < 0 || region.dstOffset.z < 0) {
        return false;
    }
    const FormatInfo srcFormat = GetFormatInfo(source->format);
    const FormatInfo dstFormat = GetFormatInfo(destination->format);
    if (srcFormat.blockWidth != dstFormat.blockWidth || srcFormat.blockHeight != dstFormat.blockHeight ||
        srcFormat.blockBytes != dstFormat.blockBytes || source->samples != destination->samples) {
        return false;
    }

    const VkExtent3D srcExtent = MipExtent(source, region.srcSubresource.mipLevel);
    const VkExtent3D dstExtent = MipExtent(destination, region.dstSubresource.mipLevel);
    const uint32_t sx = static_cast<uint32_t>(region.srcOffset.x);
    const uint32_t sy = static_cast<uint32_t>(region.srcOffset.y);
    const uint32_t sz = static_cast<uint32_t>(region.srcOffset.z);
    const uint32_t dx = static_cast<uint32_t>(region.dstOffset.x);
    const uint32_t dy = static_cast<uint32_t>(region.dstOffset.y);
    const uint32_t dz = static_cast<uint32_t>(region.dstOffset.z);
    if (sx > srcExtent.width || sy > srcExtent.height || sz > srcExtent.depth ||
        dx > dstExtent.width || dy > dstExtent.height || dz > dstExtent.depth ||
        region.extent.width > srcExtent.width - sx || region.extent.height > srcExtent.height - sy ||
        region.extent.depth > srcExtent.depth - sz || region.extent.width > dstExtent.width - dx ||
        region.extent.height > dstExtent.height - dy || region.extent.depth > dstExtent.depth - dz) {
        return false;
    }

    const uint32_t samples = static_cast<uint32_t>(source->samples);
    const VkDeviceSize srcRowPitch = static_cast<VkDeviceSize>(DivideRoundUp(srcExtent.width, srcFormat.blockWidth)) * srcFormat.blockBytes * samples;
    const VkDeviceSize dstRowPitch = static_cast<VkDeviceSize>(DivideRoundUp(dstExtent.width, dstFormat.blockWidth)) * dstFormat.blockBytes * samples;
    const VkDeviceSize srcSlicePitch = srcRowPitch * DivideRoundUp(srcExtent.height, srcFormat.blockHeight);
    const VkDeviceSize dstSlicePitch = dstRowPitch * DivideRoundUp(dstExtent.height, dstFormat.blockHeight);
    const VkDeviceSize rowBytes = static_cast<VkDeviceSize>(DivideRoundUp(region.extent.width, srcFormat.blockWidth)) * srcFormat.blockBytes * samples;
    const uint32_t rows = DivideRoundUp(region.extent.height, srcFormat.blockHeight);

    for (uint32_t layer = 0; layer < region.srcSubresource.layerCount; ++layer) {
        const uint32_t srcLayer = region.srcSubresource.baseArrayLayer + layer;
        const uint32_t dstLayer = region.dstSubresource.baseArrayLayer + layer;
        if (srcLayer >= source->arrayLayers || dstLayer >= destination->arrayLayers) return false;
        const VkDeviceSize srcBase = SubresourceOffset(source, region.srcSubresource.mipLevel, srcLayer);
        const VkDeviceSize dstBase = SubresourceOffset(destination, region.dstSubresource.mipLevel, dstLayer);
        for (uint32_t depth = 0; depth < region.extent.depth; ++depth) {
            for (uint32_t row = 0; row < rows; ++row) {
                const VkDeviceSize srcOffset = srcBase + (sz + depth) * srcSlicePitch +
                    (sy / srcFormat.blockHeight + row) * srcRowPitch + (sx / srcFormat.blockWidth) * srcFormat.blockBytes * samples;
                const VkDeviceSize dstOffset = dstBase + (dz + depth) * dstSlicePitch +
                    (dy / dstFormat.blockHeight + row) * dstRowPitch + (dx / dstFormat.blockWidth) * dstFormat.blockBytes * samples;
                uint8_t* src = nullptr;
                uint8_t* dst = nullptr;
                if (!ImagePointer(source, srcOffset, rowBytes, &src) ||
                    !ImagePointer(destination, dstOffset, rowBytes, &dst)) return false;
                std::memmove(dst, src, static_cast<size_t>(rowBytes));
            }
        }
    }
    return true;
}

bool ExecuteCommand(const nv::RecordedCommand& command) {
    switch (command.type) {
    case nv::CommandType::CopyBuffer:
        return CopyBuffer(command.srcBuffer, command.dstBuffer, command.bufferCopies);
    case nv::CommandType::UpdateBuffer: {
        uint8_t* destination = nullptr;
        if (!BufferPointer(command.dstBuffer, command.offset, command.data.size(), &destination)) return false;
        std::memcpy(destination, command.data.data(), command.data.size());
        return true;
    }
    case nv::CommandType::FillBuffer: {
        const VkDeviceSize size = command.size == VK_WHOLE_SIZE
            ? command.dstBuffer->size - command.offset : command.size;
        uint8_t* destination = nullptr;
        if (!BufferPointer(command.dstBuffer, command.offset, size, &destination) || size % 4 != 0) return false;
        for (VkDeviceSize offset = 0; offset < size; offset += 4) {
            std::memcpy(destination + static_cast<size_t>(offset), &command.fillValue, 4);
        }
        return true;
    }
    case nv::CommandType::CopyBufferToImage:
        for (const auto& region : command.bufferImageCopies) {
            if (!CopyBufferImageRegion(command.srcBuffer, command.dstImage, region, true)) return false;
        }
        return true;
    case nv::CommandType::CopyImageToBuffer:
        for (const auto& region : command.bufferImageCopies) {
            if (!CopyBufferImageRegion(command.dstBuffer, command.srcImage, region, false)) return false;
        }
        return true;
    case nv::CommandType::CopyImage:
        for (const auto& region : command.imageCopies) {
            if (!CopyImageRegion(command.srcImage, command.dstImage, region)) return false;
        }
        return true;
    case nv::CommandType::PipelineBarrier:
        for (const auto& barrier : command.imageBarriers) {
            auto* image = ToImage(barrier.image);
            if (!image) return false;
            image->currentLayout = barrier.newLayout;
        }
        return true;
    }
    return false;
}

bool ExecuteCommandBuffer(nv::NullCommandBuffer* commandBuffer) {
    if (!commandBuffer || commandBuffer->state != nv::CommandBufferState::Executable) return false;
    commandBuffer->state = nv::CommandBufferState::Pending;
    for (size_t commandIndex = 0; commandIndex < commandBuffer->commands.size(); ++commandIndex) {
        const auto& command = commandBuffer->commands[commandIndex];
        if (!ExecuteCommand(command)) {
            NV_ERR("Command %llu type=%u could not be modeled; skipped to preserve null-queue progress",
                   static_cast<unsigned long long>(commandIndex), static_cast<unsigned>(command.type));
        }
    }
    commandBuffer->state = nv::CommandBufferState::Executable;
    return true;
}

nv::NullCommandBuffer* RecordingCommandBuffer(VkCommandBuffer handle) {
    auto* commandBuffer = ToCommandBuffer(handle);
    return commandBuffer && commandBuffer->state == nv::CommandBufferState::Recording
        ? commandBuffer : nullptr;
}

void Reset(nv::NullCommandBuffer* commandBuffer) {
    commandBuffer->commands.clear();
    commandBuffer->usage = 0;
    commandBuffer->state = nv::CommandBufferState::Initial;
}

} // namespace

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkCommandPool* pCommandPool) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pCommandPool || pCreateInfo->queueFamilyIndex != 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pCommandPool = VK_NULL_HANDLE;
    auto* pool = new (std::nothrow) nv::NullCommandPool();
    if (!pool) return VK_ERROR_OUT_OF_HOST_MEMORY;
    pool->device = dev;
    pool->queueFamilyIndex = pCreateInfo->queueFamilyIndex;
    pool->flags = pCreateInfo->flags;
    pool->handle = reinterpret_cast<VkCommandPool>(pool);
    *pCommandPool = pool->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(
    VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* pool = ToCommandPool(commandPool);
    if (!dev || !pool || pool->device != dev) return;
    for (auto* commandBuffer : pool->commandBuffers) {
        commandBuffer->magic = 0;
        delete commandBuffer;
    }
    pool->commandBuffers.clear();
    pool->magic = 0;
    delete pool;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandPool(
    VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags) {
    auto* dev = ToDevice(device);
    auto* pool = ToCommandPool(commandPool);
    if (!dev || !pool || pool->device != dev) return VK_ERROR_INITIALIZATION_FAILED;
    for (auto* commandBuffer : pool->commandBuffers) Reset(commandBuffer);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer* pCommandBuffers) {
    auto* dev = ToDevice(device);
    auto* pool = pAllocateInfo ? ToCommandPool(pAllocateInfo->commandPool) : nullptr;
    if (!dev || !pool || pool->device != dev || !pCommandBuffers) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) pCommandBuffers[i] = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
        auto* commandBuffer = new (std::nothrow) nv::NullCommandBuffer();
        if (!commandBuffer) {
            vkFreeCommandBuffers(device, pAllocateInfo->commandPool, i, pCommandBuffers);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        commandBuffer->device = dev;
        commandBuffer->pool = pool;
        commandBuffer->level = pAllocateInfo->level;
        commandBuffer->handle = reinterpret_cast<VkCommandBuffer>(commandBuffer);
        pool->commandBuffers.push_back(commandBuffer);
        pCommandBuffers[i] = commandBuffer->handle;
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(
    VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers) {
    auto* dev = ToDevice(device);
    auto* pool = ToCommandPool(commandPool);
    if (!dev || !pool || pool->device != dev || (commandBufferCount != 0 && !pCommandBuffers)) return;
    for (uint32_t i = 0; i < commandBufferCount; ++i) {
        auto* commandBuffer = ToCommandBuffer(pCommandBuffers[i]);
        if (!commandBuffer || commandBuffer->pool != pool) continue;
        auto it = std::find(pool->commandBuffers.begin(), pool->commandBuffers.end(), commandBuffer);
        if (it != pool->commandBuffers.end()) pool->commandBuffers.erase(it);
        commandBuffer->magic = 0;
        delete commandBuffer;
    }
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(
    VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    auto* object = ToCommandBuffer(commandBuffer);
    if (!object || !pBeginInfo || object->state == nv::CommandBufferState::Recording ||
        object->state == nv::CommandBufferState::Pending) return VK_ERROR_INITIALIZATION_FAILED;
    object->commands.clear();
    object->usage = pBeginInfo->flags;
    object->state = nv::CommandBufferState::Recording;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    auto* object = ToCommandBuffer(commandBuffer);
    if (!object || object->state != nv::CommandBufferState::Recording) return VK_ERROR_INITIALIZATION_FAILED;
    object->state = nv::CommandBufferState::Executable;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(
    VkCommandBuffer commandBuffer, VkCommandBufferResetFlags) {
    auto* object = ToCommandBuffer(commandBuffer);
    if (!object || object->state == nv::CommandBufferState::Pending) return VK_ERROR_INITIALIZATION_FAILED;
    Reset(object);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer(
    VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer,
    uint32_t regionCount, const VkBufferCopy* pRegions) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    auto* source = ToBuffer(srcBuffer);
    auto* destination = ToBuffer(dstBuffer);
    if (!object || !source || !destination || (regionCount != 0 && !pRegions)) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::CopyBuffer;
    command.srcBuffer = source;
    command.dstBuffer = destination;
    command.bufferCopies.assign(pRegions, pRegions + regionCount);
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdUpdateBuffer(
    VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset,
    VkDeviceSize dataSize, const void* pData) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    auto* destination = ToBuffer(dstBuffer);
    if (!object || !destination || !pData || dataSize > static_cast<VkDeviceSize>(std::numeric_limits<size_t>::max())) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::UpdateBuffer;
    command.dstBuffer = destination;
    command.offset = dstOffset;
    command.data.resize(static_cast<size_t>(dataSize));
    std::memcpy(command.data.data(), pData, static_cast<size_t>(dataSize));
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdFillBuffer(
    VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset,
    VkDeviceSize size, uint32_t data) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    auto* destination = ToBuffer(dstBuffer);
    if (!object || !destination) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::FillBuffer;
    command.dstBuffer = destination;
    command.offset = dstOffset;
    command.size = size;
    command.fillValue = data;
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(
    VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage,
    VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    auto* source = ToBuffer(srcBuffer);
    auto* destination = ToImage(dstImage);
    if (!object || !source || !destination || (regionCount != 0 && !pRegions)) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::CopyBufferToImage;
    command.srcBuffer = source;
    command.dstImage = destination;
    command.dstImageLayout = dstImageLayout;
    command.bufferImageCopies.assign(pRegions, pRegions + regionCount);
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer(
    VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout,
    VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    auto* source = ToImage(srcImage);
    auto* destination = ToBuffer(dstBuffer);
    if (!object || !source || !destination || (regionCount != 0 && !pRegions)) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::CopyImageToBuffer;
    command.srcImage = source;
    command.srcImageLayout = srcImageLayout;
    command.dstBuffer = destination;
    command.bufferImageCopies.assign(pRegions, pRegions + regionCount);
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(
    VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout,
    VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount,
    const VkImageCopy* pRegions) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    auto* source = ToImage(srcImage);
    auto* destination = ToImage(dstImage);
    if (!object || !source || !destination || (regionCount != 0 && !pRegions)) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::CopyImage;
    command.srcImage = source;
    command.srcImageLayout = srcImageLayout;
    command.dstImage = destination;
    command.dstImageLayout = dstImageLayout;
    command.imageCopies.assign(pRegions, pRegions + regionCount);
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags, VkPipelineStageFlags,
    VkDependencyFlags, uint32_t, const VkMemoryBarrier*, uint32_t,
    const VkBufferMemoryBarrier*, uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers) {
    auto* object = RecordingCommandBuffer(commandBuffer);
    if (!object || (imageMemoryBarrierCount != 0 && !pImageMemoryBarriers)) return;
    nv::RecordedCommand command;
    command.type = nv::CommandType::PipelineBarrier;
    command.imageBarriers.assign(pImageMemoryBarriers, pImageMemoryBarriers + imageMemoryBarrierCount);
    for (auto& barrier : command.imageBarriers) barrier.pNext = nullptr;
    object->commands.push_back(std::move(command));
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage(
    VkCommandBuffer commandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout,
    uint32_t, const VkImageBlit*, VkFilter) {
    // Mip content is not required by the CPU-visible game flow. Barriers still
    // carry the logical layout transitions around this operation.
    (void)RecordingCommandBuffer(commandBuffer);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage2(
    VkCommandBuffer commandBuffer, const VkBlitImageInfo2*) {
    (void)RecordingCommandBuffer(commandBuffer);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage2KHR(
    VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    vkCmdBlitImage2(commandBuffer, pBlitImageInfo);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fenceHandle) {
    auto* object = ToQueue(queue);
    if (!object || (submitCount != 0 && !pSubmits)) return VK_ERROR_INITIALIZATION_FAILED;
    auto* fence = fenceHandle == VK_NULL_HANDLE
        ? nullptr : nv::ValidateHandle<nv::NullFence>(fenceHandle, nv::kMagicFence);
    if (fenceHandle != VK_NULL_HANDLE && (!fence || fence->device != object->device)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    for (uint32_t submit = 0; submit < submitCount; ++submit) {
        const auto& info = pSubmits[submit];
        if ((info.waitSemaphoreCount != 0 && (!info.pWaitSemaphores || !info.pWaitDstStageMask)) ||
            (info.commandBufferCount != 0 && !info.pCommandBuffers) ||
            (info.signalSemaphoreCount != 0 && !info.pSignalSemaphores)) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        for (uint32_t i = 0; i < info.waitSemaphoreCount; ++i) {
            auto* semaphore = nv::ValidateHandle<nv::NullSemaphore>(
                info.pWaitSemaphores[i], nv::kMagicSemaphore);
            if (!semaphore || semaphore->device != object->device || !nv::ConsumeSemaphore(semaphore)) {
                return VK_ERROR_DEVICE_LOST;
            }
        }
        for (uint32_t i = 0; i < info.commandBufferCount; ++i) {
            auto* commandBuffer = ToCommandBuffer(info.pCommandBuffers[i]);
            if (!commandBuffer || commandBuffer->device != object->device ||
                !ExecuteCommandBuffer(commandBuffer)) {
                NV_ERR("vkQueueSubmit failed while executing command buffer %u", i);
                return VK_ERROR_DEVICE_LOST;
            }
        }
        for (uint32_t i = 0; i < info.signalSemaphoreCount; ++i) {
            auto* semaphore = nv::ValidateHandle<nv::NullSemaphore>(
                info.pSignalSemaphores[i], nv::kMagicSemaphore);
            if (!semaphore || semaphore->device != object->device) return VK_ERROR_DEVICE_LOST;
            nv::SignalSemaphore(semaphore);
        }
    }
    nv::SignalFence(fence);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {
    return ToQueue(queue) ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
    return ToDevice(device) ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
}
