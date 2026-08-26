// nv_objects.cpp
#include "nv_objects.h"
#include "nv_log.h"
#include <cstring>
#include <cstdlib>

namespace nv {

// ---------------------------------------------------------------------------
// Instance extensions the game looks for
// ---------------------------------------------------------------------------
static const char* kInstanceExtNames[] = {
    "VK_KHR_surface",
    "VK_KHR_win32_surface",
    "VK_KHR_get_surface_capabilities2",
    "VK_EXT_debug_utils",
    "VK_KHR_get_physical_device_properties2",
    "VK_KHR_external_memory_capabilities",
    "VK_EXT_swapchain_colorspace",
};
static constexpr int kInstanceExtCount = sizeof(kInstanceExtNames) / sizeof(kInstanceExtNames[0]);

// ---------------------------------------------------------------------------
// Device extensions the game enables
// ---------------------------------------------------------------------------
static const char* kDeviceExtNames[] = {
    "VK_KHR_swapchain",
    "VK_KHR_shared_presentable_image",
    "VK_KHR_external_memory",
    "VK_KHR_external_memory_win32",
    "VK_KHR_dedicated_allocation",
    "VK_KHR_bind_memory2",
    "VK_KHR_buffer_device_address",
    "VK_EXT_memory_budget",
    "VK_KHR_16bit_storage",
    "VK_KHR_shader_float16_int8",
    "VK_KHR_format_feature_flags2",
    "VK_KHR_maintenance4",
    "VK_KHR_sampler_ycbcr_conversion",
    "VK_EXT_sampler_filter_minmax",
    "VK_NV_linear_color_attachment",
    "VK_NV_device_diagnostic_checkpoints",
};
static constexpr int kDeviceExtCount = sizeof(kDeviceExtNames) / sizeof(kDeviceExtNames[0]);

// ---------------------------------------------------------------------------
// Build extension property lists
// ---------------------------------------------------------------------------
static void BuildExtensionList(const char* names[], int count, std::vector<VkExtensionProperties>& out) {
    out.clear();
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        VkExtensionProperties ep{};
        strncpy_s(ep.extensionName, sizeof(ep.extensionName), names[i], _TRUNCATE);
        ep.specVersion = 1;
        out.push_back(ep);
    }
}

// ---------------------------------------------------------------------------
// Initialize physical device properties / features / memory
// ---------------------------------------------------------------------------
void InitPhysicalDevice(NullPhysicalDevice* pd) {
    if (!pd) return;

    // -- Properties --
    pd->props.apiVersion = VK_API_VERSION_1_3;
    pd->props.driverVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    pd->props.vendorID = 0x10DE; // NVIDIA
    pd->props.deviceID = 0x1C03; // GTX 1060 (harmless, not in known table? rating 4 fallback works)
    pd->props.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    strncpy_s(pd->props.deviceName, sizeof(pd->props.deviceName), "Null Sky GPU (NVIDIA compatible)", _TRUNCATE);
    // Pipeline ID
    pd->props.pipelineCacheUUID[0] = 'N'; pd->props.pipelineCacheUUID[1] = 'V';
    pd->props.pipelineCacheUUID[2] = 'N'; pd->props.pipelineCacheUUID[3] = 'U';
    pd->props.pipelineCacheUUID[4] = 'L'; pd->props.pipelineCacheUUID[5] = 'L';

    // -- Limits (generous defaults) --
    auto& lim = pd->props.limits;
    lim.maxImageDimension1D = 16384;
    lim.maxImageDimension2D = 16384;
    lim.maxImageDimension3D = 2048;
    lim.maxImageDimensionCube = 16384;
    lim.maxImageArrayLayers = 2048;
    lim.maxTexelBufferElements = 0x1000000;
    lim.maxUniformBufferRange = 65536;
    lim.maxStorageBufferRange = 0x40000000;
    lim.maxPushConstantsSize = 128;
    lim.maxMemoryAllocationCount = 4096;
    lim.maxSamplerAllocationCount = 4096;
    lim.bufferImageGranularity = 1024;
    lim.sparseAddressSpaceSize = 0;
    lim.maxBoundDescriptorSets = 32;
    lim.maxPerStageDescriptorSamplers = 64;
    lim.maxPerStageDescriptorUniformBuffers = 64;
    lim.maxPerStageDescriptorStorageBuffers = 64;
    lim.maxPerStageDescriptorSampledImages = 256;
    lim.maxPerStageDescriptorStorageImages = 64;
    lim.maxPerStageDescriptorInputAttachments = 64;
    lim.maxPerStageResources = 512;
    lim.maxDescriptorSetSamplers = 256;
    lim.maxDescriptorSetUniformBuffers = 256;
    lim.maxDescriptorSetUniformBuffersDynamic = 16;
    lim.maxDescriptorSetStorageBuffers = 256;
    lim.maxDescriptorSetStorageBuffersDynamic = 16;
    lim.maxDescriptorSetSampledImages = 1024;
    lim.maxDescriptorSetStorageImages = 256;
    lim.maxDescriptorSetInputAttachments = 256;
    lim.maxVertexInputAttributes = 32;
    lim.maxVertexInputBindings = 32;
    lim.maxVertexInputAttributeOffset = 2047;
    lim.maxVertexInputBindingStride = 2048;
    lim.maxVertexOutputComponents = 128;
    lim.maxTessellationGenerationLevel = 64;
    lim.maxTessellationPatchSize = 32;
    lim.maxTessellationControlPerVertexInputComponents = 128;
    lim.maxTessellationControlPerVertexOutputComponents = 128;
    lim.maxTessellationControlPerPatchOutputComponents = 128;
    lim.maxTessellationControlTotalOutputComponents = 2048;
    lim.maxTessellationEvaluationInputComponents = 128;
    lim.maxTessellationEvaluationOutputComponents = 128;
    lim.maxGeometryShaderInvocations = 32;
    lim.maxGeometryInputComponents = 128;
    lim.maxGeometryOutputComponents = 128;
    lim.maxGeometryOutputVertices = 256;
    lim.maxGeometryTotalOutputComponents = 1024;
    lim.maxFragmentInputComponents = 128;
    lim.maxFragmentOutputAttachments = 8;
    lim.maxFragmentDualSrcAttachments = 1;
    lim.maxFragmentCombinedOutputResources = 8;
    lim.maxComputeSharedMemorySize = 32768;
    lim.maxComputeWorkGroupCount[0] = 65535;
    lim.maxComputeWorkGroupCount[1] = 65535;
    lim.maxComputeWorkGroupCount[2] = 65535;
    lim.maxComputeWorkGroupInvocations = 1024;
    lim.maxComputeWorkGroupSize[0] = 1024;
    lim.maxComputeWorkGroupSize[1] = 1024;
    lim.maxComputeWorkGroupSize[2] = 64;
    lim.subPixelPrecisionBits = 4;
    lim.subTexelPrecisionBits = 4;
    lim.mipmapPrecisionBits = 4;
    lim.maxDrawIndexedIndexValue = 0xFFFFFFFF;
    lim.maxDrawIndirectCount = 0xFFFFFFFF;
    lim.maxSamplerLodBias = 15.0f;
    lim.maxSamplerAnisotropy = 16.0f;
    lim.maxViewports = 16;
    lim.maxViewportDimensions[0] = 16384;
    lim.maxViewportDimensions[1] = 16384;
    lim.viewportBoundsRange[0] = -32768.0f;
    lim.viewportBoundsRange[1] = 32767.0f;
    lim.viewportSubPixelBits = 13;
    lim.minMemoryMapAlignment = 64;
    lim.minTexelBufferOffsetAlignment = 256;
    lim.minUniformBufferOffsetAlignment = 256;
    lim.minStorageBufferOffsetAlignment = 256;
    lim.minTexelOffset = -8;
    lim.maxTexelOffset = 7;
    lim.minTexelGatherOffset = -8;
    lim.maxTexelGatherOffset = 7;
    lim.minInterpolationOffset = -0.5f;
    lim.maxInterpolationOffset = 0.5f;
    lim.subPixelInterpolationOffsetBits = 4;
    lim.maxFramebufferWidth = 16384;
    lim.maxFramebufferHeight = 16384;
    lim.maxFramebufferLayers = 1024;
    lim.framebufferColorSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.framebufferDepthSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.framebufferStencilSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.maxColorAttachments = 8;
    lim.sampledImageColorSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.sampledImageIntegerSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.sampledImageDepthSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.sampledImageStencilSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.storageImageSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    lim.maxSampleMaskWords = 1;
    lim.timestampComputeAndGraphics = VK_TRUE;
    lim.timestampPeriod = 1.0f;
    lim.maxClipDistances = 8;
    lim.maxCullDistances = 8;
    lim.maxCombinedClipAndCullDistances = 8;
    lim.discreteQueuePriorities = 2;
    lim.pointSizeGranularity = 1.0f;
    lim.lineWidthGranularity = 1.0f;
    lim.strictLines = VK_FALSE;
    lim.standardSampleLocations = VK_TRUE;
    lim.optimalBufferCopyOffsetAlignment = 256;
    lim.optimalBufferCopyRowPitchAlignment = 256;
    lim.nonCoherentAtomSize = 256;

    // -- Sparse --
    lim.sparseAddressSpaceSize = 0;
    lim.maxSamplerAnisotropy = 16.0f;

    // -- Features (Vulkan 1.0) --
    VkPhysicalDeviceFeatures& feat = pd->features.features;
    feat.robustBufferAccess = VK_TRUE;
    feat.fullDrawIndexUint32 = VK_TRUE;
    feat.imageCubeArray = VK_TRUE;
    feat.independentBlend = VK_TRUE;
    feat.geometryShader = VK_TRUE;
    feat.tessellationShader = VK_TRUE;
    feat.sampleRateShading = VK_TRUE;
    feat.dualSrcBlend = VK_TRUE;
    feat.logicOp = VK_TRUE;
    feat.multiDrawIndirect = VK_TRUE;
    feat.drawIndirectFirstInstance = VK_TRUE;
    feat.depthClamp = VK_TRUE;
    feat.depthBiasClamp = VK_TRUE;
    feat.fillModeNonSolid = VK_TRUE;
    feat.depthBounds = VK_TRUE;
    feat.wideLines = VK_TRUE;
    feat.largePoints = VK_TRUE;
    feat.alphaToOne = VK_TRUE;
    feat.multiViewport = VK_TRUE;
    feat.samplerAnisotropy = VK_TRUE;
    feat.textureCompressionETC2 = VK_FALSE;
    feat.textureCompressionASTC_LDR = VK_FALSE;
    feat.textureCompressionBC = VK_TRUE;
    feat.occlusionQueryPrecise = VK_TRUE;
    feat.pipelineStatisticsQuery = VK_FALSE;
    feat.vertexPipelineStoresAndAtomics = VK_TRUE;
    feat.fragmentStoresAndAtomics = VK_TRUE;
    feat.shaderTessellationAndGeometryPointSize = VK_TRUE;
    feat.shaderImageGatherExtended = VK_TRUE;
    feat.shaderStorageImageExtendedFormats = VK_TRUE;
    feat.shaderStorageImageMultisample = VK_TRUE;
    feat.shaderStorageImageReadWithoutFormat = VK_TRUE;
    feat.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    feat.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
    feat.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    feat.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
    feat.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
    feat.shaderClipDistance = VK_TRUE;
    feat.shaderCullDistance = VK_TRUE;
    feat.shaderFloat64 = VK_TRUE;
    feat.shaderInt64 = VK_TRUE;
    feat.shaderInt16 = VK_TRUE;
    feat.shaderResourceResidency = VK_FALSE;
    feat.shaderResourceMinLod = VK_FALSE;
    feat.sparseBinding = VK_FALSE;
    feat.sparseResidencyBuffer = VK_FALSE;
    feat.sparseResidencyImage2D = VK_FALSE;
    feat.sparseResidencyImage3D = VK_FALSE;
    feat.sparseResidency2Samples = VK_FALSE;
    feat.sparseResidency4Samples = VK_FALSE;
    feat.sparseResidency8Samples = VK_FALSE;
    feat.sparseResidency16Samples = VK_FALSE;
    feat.sparseResidencyAliased = VK_FALSE;
    feat.variableMultisampleRate = VK_FALSE;
    feat.inheritedQueries = VK_FALSE;

    // -- Vulkan 1.1 features --
    pd->vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    pd->vk11Features.pNext = &pd->vk12Features;
    pd->vk11Features.storageBuffer16BitAccess = VK_TRUE;
    pd->vk11Features.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    pd->vk11Features.storagePushConstant16 = VK_TRUE;
    pd->vk11Features.storageInputOutput16 = VK_TRUE;
    pd->vk11Features.multiview = VK_FALSE;
    pd->vk11Features.multiviewGeometryShader = VK_FALSE;
    pd->vk11Features.multiviewTessellationShader = VK_FALSE;
    pd->vk11Features.variablePointersStorageBuffer = VK_TRUE;
    pd->vk11Features.variablePointers = VK_TRUE;
    pd->vk11Features.protectedMemory = VK_FALSE;
    pd->vk11Features.samplerYcbcrConversion = VK_TRUE;
    pd->vk11Features.shaderDrawParameters = VK_TRUE;

    // -- Vulkan 1.2 features --
    pd->vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    pd->vk12Features.pNext = &pd->vk13Features;
    pd->vk12Features.drawIndirectCount = VK_FALSE;
    pd->vk12Features.samplerFilterMinmax = VK_TRUE;
    pd->vk12Features.shaderOutputViewportIndex = VK_TRUE;
    pd->vk12Features.shaderOutputLayer = VK_TRUE;
    pd->vk12Features.subgroupBroadcastDynamicId = VK_TRUE;
    pd->vk12Features.descriptorBindingPartiallyBound = VK_TRUE;
    pd->vk12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    pd->vk12Features.runtimeDescriptorArray = VK_TRUE;
    pd->vk12Features.samplerFilterMinmax = VK_TRUE;
    pd->vk12Features.scalarBlockLayout = VK_TRUE;
    pd->vk12Features.imagelessFramebuffer = VK_FALSE;
    pd->vk12Features.uniformBufferStandardLayout = VK_TRUE;
    pd->vk12Features.shaderSubgroupExtendedTypes = VK_TRUE;
    pd->vk12Features.separateDepthStencilLayouts = VK_TRUE;
    pd->vk12Features.hostQueryReset = VK_TRUE;
    pd->vk12Features.timelineSemaphore = VK_FALSE;
    pd->vk12Features.bufferDeviceAddress = VK_TRUE; // CRITICAL
    pd->vk12Features.bufferDeviceAddressCaptureReplay = VK_FALSE;
    pd->vk12Features.bufferDeviceAddressMultiDevice = VK_FALSE;
    pd->vk12Features.vulkanMemoryModel = VK_FALSE;
    pd->vk12Features.vulkanMemoryModelDeviceScope = VK_FALSE;
    pd->vk12Features.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
    pd->vk12Features.shaderOutputViewportIndex = VK_TRUE;
    pd->vk12Features.shaderOutputLayer = VK_TRUE;
    pd->vk12Features.subgroupBroadcastDynamicId = VK_TRUE;
    pd->vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    pd->vk12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    pd->vk12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    pd->vk12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    pd->vk12Features.descriptorBindingPartiallyBound = VK_TRUE;
    pd->vk12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    pd->vk12Features.runtimeDescriptorArray = VK_TRUE;
    pd->vk12Features.samplerFilterMinmax = VK_TRUE;
    pd->vk12Features.scalarBlockLayout = VK_TRUE;
    pd->vk12Features.imagelessFramebuffer = VK_FALSE;
    pd->vk12Features.uniformBufferStandardLayout = VK_TRUE;
    pd->vk12Features.shaderSubgroupExtendedTypes = VK_TRUE;
    pd->vk12Features.separateDepthStencilLayouts = VK_TRUE;
    pd->vk12Features.hostQueryReset = VK_TRUE;
    pd->vk12Features.timelineSemaphore = VK_FALSE;
    pd->vk12Features.bufferDeviceAddress = VK_TRUE;
    pd->vk12Features.bufferDeviceAddressCaptureReplay = VK_FALSE;
    pd->vk12Features.bufferDeviceAddressMultiDevice = VK_FALSE;
    pd->vk12Features.vulkanMemoryModel = VK_FALSE;
    pd->vk12Features.vulkanMemoryModelDeviceScope = VK_FALSE;
    pd->vk12Features.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;

    // -- Vulkan 1.3 features --
    pd->vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    pd->vk13Features.pNext = nullptr;
    pd->vk13Features.robustImageAccess = VK_FALSE;
    pd->vk13Features.inlineUniformBlock = VK_FALSE;
    pd->vk13Features.descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE;
    pd->vk13Features.pipelineCreationCacheControl = VK_TRUE;
    pd->vk13Features.privateData = VK_FALSE;
    pd->vk13Features.shaderDemoteToHelperInvocation = VK_TRUE;
    pd->vk13Features.shaderTerminateInvocation = VK_TRUE;
    pd->vk13Features.subgroupSizeControl = VK_TRUE;
    pd->vk13Features.computeFullSubgroups = VK_TRUE;
    pd->vk13Features.synchronization2 = VK_FALSE;
    pd->vk13Features.textureCompressionASTC_HDR = VK_FALSE;
    pd->vk13Features.shaderZeroInitializeWorkgroupMemory = VK_FALSE;
    pd->vk13Features.dynamicRendering = VK_FALSE; // game doesn't use
    pd->vk13Features.shaderIntegerDotProduct = VK_TRUE;
    pd->vk13Features.maintenance4 = VK_TRUE;

    // -- Features2 root --
    pd->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    pd->features.pNext = &pd->vk11Features;

    // -- Queue families --
    pd->queueFamilies[0].queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    pd->queueFamilies[0].queueCount = 1;
    pd->queueFamilies[0].timestampValidBits = 64;
    pd->queueFamilies[0].minImageTransferGranularity = {1, 1, 1};

    // -- Memory properties --
    pd->memProps.memoryTypeCount = 2;
    pd->memProps.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    pd->memProps.memoryTypes[0].heapIndex = 0;
    pd->memProps.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    pd->memProps.memoryTypes[1].heapIndex = 1;
    pd->memProps.memoryHeapCount = 2;
    pd->memProps.memoryHeaps[0].size = 8ULL * 1024 * 1024 * 1024; // 8 GB device
    pd->memProps.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    pd->memProps.memoryHeaps[1].size = 16ULL * 1024 * 1024 * 1024; // 16 GB host

    // -- Extensions --
    BuildExtensionList(kInstanceExtNames, kInstanceExtCount, pd->instanceExtensions);
    BuildExtensionList(kDeviceExtNames, kDeviceExtCount, pd->deviceExtensions);
}

} // namespace nv
