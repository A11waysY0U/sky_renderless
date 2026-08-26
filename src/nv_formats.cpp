// Physical-device format capability queries used during renderer initialization.

#include "nv_log.h"
#include "nv_objects.h"

#include <algorithm>
#include <cstring>

namespace {

nv::NullPhysicalDevice* ToPhysicalDevice(VkPhysicalDevice handle) {
    return nv::ValidateHandle<nv::NullPhysicalDevice>(handle, nv::kMagicPhysicalDevice);
}

bool IsDepthStencilFormat(VkFormat format) {
    return format >= VK_FORMAT_D16_UNORM && format <= VK_FORMAT_D32_SFLOAT_S8_UINT;
}

bool IsCompressedFormat(VkFormat format) {
    return (format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK && format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK) ||
           (format >= VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG &&
            format <= VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
}

bool IsMultiPlanarFormat(VkFormat format) {
    return format >= VK_FORMAT_G8B8G8R8_422_UNORM &&
           format <= VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM;
}

bool IsKnownFormat(VkFormat format) {
    return format != VK_FORMAT_UNDEFINED && format != VK_FORMAT_MAX_ENUM;
}

VkFormatFeatureFlags CommonTransferFeatures() {
    return VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
           VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
}

VkFormatProperties FormatProperties(VkFormat format) {
    VkFormatProperties properties{};
    if (!IsKnownFormat(format)) return properties;

    const VkFormatFeatureFlags sampled = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
    const VkFormatFeatureFlags buffers = VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT |
        VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT | VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;

    if (IsDepthStencilFormat(format)) {
        properties.linearTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                          CommonTransferFeatures();
        properties.optimalTilingFeatures = properties.linearTilingFeatures |
                                            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        return properties;
    }
    if (IsCompressedFormat(format)) {
        properties.optimalTilingFeatures = sampled | CommonTransferFeatures();
        return properties;
    }
    if (IsMultiPlanarFormat(format)) {
        properties.optimalTilingFeatures = sampled | CommonTransferFeatures() |
            VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
            VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT;
        return properties;
    }

    const VkFormatFeatureFlags color = sampled | CommonTransferFeatures() |
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    properties.linearTilingFeatures = color;
    properties.optimalTilingFeatures = color;
    properties.bufferFeatures = buffers;
    return properties;
}

VkSampleCountFlags SupportedSamples(VkFormat format, VkImageTiling tiling) {
    if (tiling == VK_IMAGE_TILING_LINEAR || IsCompressedFormat(format) || IsMultiPlanarFormat(format)) {
        return VK_SAMPLE_COUNT_1_BIT;
    }
    return VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
}

VkResult ImageFormatProperties(VkFormat format, VkImageType type, VkImageTiling tiling,
                               VkImageUsageFlags usage, VkImageCreateFlags flags,
                               VkImageFormatProperties* output) {
    if (!output) return VK_ERROR_INITIALIZATION_FAILED;
    *output = {};
    if (!IsKnownFormat(format) || type > VK_IMAGE_TYPE_3D ||
        (flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
                  VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)) != 0) {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    const VkFormatProperties features = FormatProperties(format);
    const VkFormatFeatureFlags tilingFeatures = tiling == VK_IMAGE_TILING_LINEAR
        ? features.linearTilingFeatures : features.optimalTilingFeatures;
    if (tilingFeatures == 0) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) &&
        !(tilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) &&
        !(tilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
        !(tilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
        !(tilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;

    output->maxExtent = type == VK_IMAGE_TYPE_3D
        ? VkExtent3D{2048, 2048, 2048} : VkExtent3D{16384, 16384, 1};
    if (type == VK_IMAGE_TYPE_1D) output->maxExtent.height = 1;
    output->maxMipLevels = type == VK_IMAGE_TYPE_3D ? 12 : 15;
    output->maxArrayLayers = type == VK_IMAGE_TYPE_3D ? 1 : 2048;
    output->sampleCounts = SupportedSamples(format, tiling);
    output->maxResourceSize = 4ull * 1024 * 1024 * 1024;
    return VK_SUCCESS;
}

void FillFormatPropertiesPNext(void* pNext, const VkFormatProperties& properties) {
    for (auto* current = reinterpret_cast<VkBaseOutStructure*>(pNext);
         current; current = current->pNext) {
        if (current->sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3) {
            auto* format3 = reinterpret_cast<VkFormatProperties3*>(current);
            format3->linearTilingFeatures = properties.linearTilingFeatures;
            format3->optimalTilingFeatures = properties.optimalTilingFeatures;
            format3->bufferFeatures = properties.bufferFeatures;
        }
    }
}

void FillImagePropertiesPNext(void* pNext) {
    for (auto* current = reinterpret_cast<VkBaseOutStructure*>(pNext);
         current; current = current->pNext) {
        switch (current->sType) {
        case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES: {
            auto* external = reinterpret_cast<VkExternalImageFormatProperties*>(current);
            external->externalMemoryProperties = {};
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES: {
            auto* ycbcr = reinterpret_cast<VkSamplerYcbcrConversionImageFormatProperties*>(current);
            ycbcr->combinedImageSamplerDescriptorCount = 1;
            break;
        }
        default:
            break;
        }
    }
}

} // namespace

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    if (!ToPhysicalDevice(physicalDevice) || !pFormatProperties) return;
    *pFormatProperties = FormatProperties(format);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2(
    VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    if (!ToPhysicalDevice(physicalDevice) || !pFormatProperties) return;
    pFormatProperties->formatProperties = FormatProperties(format);
    FillFormatPropertiesPNext(pFormatProperties->pNext, pFormatProperties->formatProperties);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2KHR(
    VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
    VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkImageFormatProperties* pImageFormatProperties) {
    if (!ToPhysicalDevice(physicalDevice)) return VK_ERROR_INITIALIZATION_FAILED;
    return ImageFormatProperties(format, type, tiling, usage, flags, pImageFormatProperties);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo,
    VkImageFormatProperties2* pImageFormatProperties) {
    if (!ToPhysicalDevice(physicalDevice) || !pImageFormatInfo || !pImageFormatProperties) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult result = ImageFormatProperties(
        pImageFormatInfo->format, pImageFormatInfo->type, pImageFormatInfo->tiling,
        pImageFormatInfo->usage, pImageFormatInfo->flags,
        &pImageFormatProperties->imageFormatProperties);
    if (result == VK_SUCCESS) FillImagePropertiesPNext(pImageFormatProperties->pNext);
    return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo,
    VkImageFormatProperties2* pImageFormatProperties) {
    return vkGetPhysicalDeviceImageFormatProperties2(
        physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice, VkFormat, VkImageType, VkSampleCountFlagBits,
    VkImageUsageFlags, VkImageTiling, uint32_t* pPropertyCount,
    VkSparseImageFormatProperties*) {
    if (pPropertyCount) *pPropertyCount = 0;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2*,
    uint32_t* pPropertyCount, VkSparseImageFormatProperties2*) {
    if (pPropertyCount) *pPropertyCount = 0;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo,
    uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    vkGetPhysicalDeviceSparseImageFormatProperties2(
        physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}
