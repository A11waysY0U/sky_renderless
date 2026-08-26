// Renderer initialization objects. Draw and dispatch commands are intentionally discarded.

#include "nv_objects.h"

#include <algorithm>
#include <cstring>
#include <new>

namespace {

nv::NullDevice* ToDevice(VkDevice handle) {
    return nv::ValidateHandle<nv::NullDevice>(handle, nv::kMagicDevice);
}

nv::NullImage* ToImage(VkImage handle) {
    return nv::ValidateHandle<nv::NullImage>(handle, nv::kMagicImage);
}

nv::NullBuffer* ToBuffer(VkBuffer handle) {
    return nv::ValidateHandle<nv::NullBuffer>(handle, nv::kMagicBuffer);
}

nv::NullImageView* ToImageView(VkImageView handle) {
    return nv::ValidateHandle<nv::NullImageView>(handle, nv::kMagicImageView);
}

nv::NullBufferView* ToBufferView(VkBufferView handle) {
    return nv::ValidateHandle<nv::NullBufferView>(handle, nv::kMagicBufferView);
}

nv::NullDescriptorSetLayout* ToSetLayout(VkDescriptorSetLayout handle) {
    return nv::ValidateHandle<nv::NullDescriptorSetLayout>(handle, nv::kMagicSetLayout);
}

nv::NullDescriptorPool* ToDescriptorPool(VkDescriptorPool handle) {
    return nv::ValidateHandle<nv::NullDescriptorPool>(handle, nv::kMagicDescriptorPool);
}

nv::NullDescriptorSet* ToDescriptorSet(VkDescriptorSet handle) {
    return nv::ValidateHandle<nv::NullDescriptorSet>(handle, nv::kMagicDescriptorSet);
}

nv::NullPipelineLayout* ToPipelineLayout(VkPipelineLayout handle) {
    return nv::ValidateHandle<nv::NullPipelineLayout>(handle, nv::kMagicPipelineLayout);
}

nv::NullPipelineCache* ToPipelineCache(VkPipelineCache handle) {
    return nv::ValidateHandle<nv::NullPipelineCache>(handle, nv::kMagicPipelineCache);
}

nv::NullRenderPass* ToRenderPass(VkRenderPass handle) {
    return nv::ValidateHandle<nv::NullRenderPass>(handle, nv::kMagicRenderPass);
}

nv::NullCommandBuffer* RecordingCommandBuffer(VkCommandBuffer handle) {
    auto* commandBuffer = nv::ValidateHandle<nv::NullCommandBuffer>(handle, nv::kMagicCommandBuffer);
    return commandBuffer && commandBuffer->state == nv::CommandBufferState::Recording
        ? commandBuffer : nullptr;
}

nv::DescriptorBindingState* FindBinding(nv::NullDescriptorSet* set, uint32_t binding) {
    for (auto& state : set->bindings) {
        if (state.binding == binding) return &state;
    }
    return nullptr;
}

void DeleteDescriptorSet(nv::NullDescriptorSet* set) {
    set->magic = 0;
    delete set;
}

void ResetDescriptorPool(nv::NullDescriptorPool* pool) {
    for (auto* set : pool->sets) DeleteDescriptorSet(set);
    pool->sets.clear();
}

template <typename CreateInfo>
VkResult CreateRenderPassObject(VkDevice device, const CreateInfo* info, VkRenderPass* output) {
    auto* dev = ToDevice(device);
    if (!dev || !info || !output || info->subpassCount == 0 || !info->pSubpasses) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *output = VK_NULL_HANDLE;
    auto* renderPass = new (std::nothrow) nv::NullRenderPass();
    if (!renderPass) return VK_ERROR_OUT_OF_HOST_MEMORY;
    renderPass->device = dev;
    renderPass->attachmentCount = info->attachmentCount;
    renderPass->subpassCount = info->subpassCount;
    renderPass->handle = reinterpret_cast<VkRenderPass>(renderPass);
    *output = renderPass->handle;
    return VK_SUCCESS;
}

VkResult CreatePipeline(nv::NullDevice* device, VkPipelineBindPoint bindPoint,
                        VkPipelineLayout layoutHandle, VkPipeline* output) {
    if (!output) return VK_ERROR_INITIALIZATION_FAILED;
    *output = VK_NULL_HANDLE;
    auto* layout = ToPipelineLayout(layoutHandle);
    if (!layout || layout->device != device) return VK_ERROR_INITIALIZATION_FAILED;
    auto* pipeline = new (std::nothrow) nv::NullPipeline();
    if (!pipeline) return VK_ERROR_OUT_OF_HOST_MEMORY;
    pipeline->device = device;
    pipeline->bindPoint = bindPoint;
    pipeline->layout = layout;
    pipeline->handle = reinterpret_cast<VkPipeline>(pipeline);
    *output = pipeline->handle;
    return VK_SUCCESS;
}

} // namespace

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSamplerYcbcrConversion(
    VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkSamplerYcbcrConversion* pYcbcrConversion) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pYcbcrConversion) return VK_ERROR_INITIALIZATION_FAILED;
    *pYcbcrConversion = VK_NULL_HANDLE;
    auto* conversion = new (std::nothrow) nv::NullSamplerYcbcrConversion();
    if (!conversion) return VK_ERROR_OUT_OF_HOST_MEMORY;
    conversion->device = dev;
    conversion->format = pCreateInfo->format;
    conversion->handle = reinterpret_cast<VkSamplerYcbcrConversion>(conversion);
    *pYcbcrConversion = conversion->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSamplerYcbcrConversionKHR(
    VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* allocator, VkSamplerYcbcrConversion* output) {
    return vkCreateSamplerYcbcrConversion(device, pCreateInfo, allocator, output);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversion(
    VkDevice device, VkSamplerYcbcrConversion conversion, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = nv::ValidateHandle<nv::NullSamplerYcbcrConversion>(conversion, nv::kMagicYcbcr);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversionKHR(
    VkDevice device, VkSamplerYcbcrConversion conversion, const VkAllocationCallbacks* allocator) {
    vkDestroySamplerYcbcrConversion(device, conversion, allocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(
    VkDevice device, const VkSamplerCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkSampler* pSampler) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pSampler) return VK_ERROR_INITIALIZATION_FAILED;
    *pSampler = VK_NULL_HANDLE;
    auto* sampler = new (std::nothrow) nv::NullSampler();
    if (!sampler) return VK_ERROR_OUT_OF_HOST_MEMORY;
    sampler->device = dev;
    sampler->createInfo = *pCreateInfo;
    sampler->createInfo.pNext = nullptr;
    sampler->handle = reinterpret_cast<VkSampler>(sampler);
    *pSampler = sampler->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySampler(
    VkDevice device, VkSampler sampler, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = nv::ValidateHandle<nv::NullSampler>(sampler, nv::kMagicSampler);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(
    VkDevice device, const VkImageViewCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkImageView* pView) {
    auto* dev = ToDevice(device);
    auto* image = pCreateInfo ? ToImage(pCreateInfo->image) : nullptr;
    if (!dev || !image || image->device != dev || !pView) return VK_ERROR_INITIALIZATION_FAILED;
    *pView = VK_NULL_HANDLE;
    auto* view = new (std::nothrow) nv::NullImageView();
    if (!view) return VK_ERROR_OUT_OF_HOST_MEMORY;
    view->device = dev;
    view->image = image;
    view->viewType = pCreateInfo->viewType;
    view->format = pCreateInfo->format;
    view->subresourceRange = pCreateInfo->subresourceRange;
    view->handle = reinterpret_cast<VkImageView>(view);
    *pView = view->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(
    VkDevice device, VkImageView imageView, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* view = ToImageView(imageView);
    if (!dev || !view || view->device != dev) return;
    view->magic = 0;
    delete view;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBufferView(
    VkDevice device, const VkBufferViewCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkBufferView* pView) {
    auto* dev = ToDevice(device);
    auto* buffer = pCreateInfo ? ToBuffer(pCreateInfo->buffer) : nullptr;
    if (!dev || !buffer || buffer->device != dev || !pView || pCreateInfo->offset > buffer->size) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const VkDeviceSize range = pCreateInfo->range == VK_WHOLE_SIZE
        ? buffer->size - pCreateInfo->offset : pCreateInfo->range;
    if (range > buffer->size - pCreateInfo->offset) return VK_ERROR_INITIALIZATION_FAILED;
    *pView = VK_NULL_HANDLE;
    auto* view = new (std::nothrow) nv::NullBufferView();
    if (!view) return VK_ERROR_OUT_OF_HOST_MEMORY;
    view->device = dev;
    view->buffer = buffer;
    view->format = pCreateInfo->format;
    view->offset = pCreateInfo->offset;
    view->range = range;
    view->handle = reinterpret_cast<VkBufferView>(view);
    *pView = view->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyBufferView(
    VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* view = ToBufferView(bufferView);
    if (!dev || !view || view->device != dev) return;
    view->magic = 0;
    delete view;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkShaderModule* pShaderModule) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pShaderModule || !pCreateInfo->pCode ||
        pCreateInfo->codeSize == 0 || pCreateInfo->codeSize % sizeof(uint32_t) != 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pShaderModule = VK_NULL_HANDLE;
    auto* module = new (std::nothrow) nv::NullShaderModule();
    if (!module) return VK_ERROR_OUT_OF_HOST_MEMORY;
    module->device = dev;
    module->code.assign(pCreateInfo->pCode, pCreateInfo->pCode + pCreateInfo->codeSize / sizeof(uint32_t));
    module->handle = reinterpret_cast<VkShaderModule>(module);
    *pShaderModule = module->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(
    VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* module = nv::ValidateHandle<nv::NullShaderModule>(shaderModule, nv::kMagicShaderModule);
    if (!dev || !module || module->device != dev) return;
    module->magic = 0;
    delete module;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkDescriptorSetLayout* pSetLayout) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pSetLayout ||
        (pCreateInfo->bindingCount != 0 && !pCreateInfo->pBindings)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pSetLayout = VK_NULL_HANDLE;
    auto* layout = new (std::nothrow) nv::NullDescriptorSetLayout();
    if (!layout) return VK_ERROR_OUT_OF_HOST_MEMORY;
    layout->device = dev;
    layout->flags = pCreateInfo->flags;
    layout->bindings.assign(pCreateInfo->pBindings, pCreateInfo->pBindings + pCreateInfo->bindingCount);
    for (auto& binding : layout->bindings) binding.pImmutableSamplers = nullptr;
    layout->handle = reinterpret_cast<VkDescriptorSetLayout>(layout);
    *pSetLayout = layout->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* layout = ToSetLayout(descriptorSetLayout);
    if (!dev || !layout || layout->device != dev) return;
    layout->magic = 0;
    delete layout;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupport(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
    VkDescriptorSetLayoutSupport* pSupport) {
    if (pSupport) pSupport->supported = ToDevice(device) && pCreateInfo ? VK_TRUE : VK_FALSE;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupportKHR(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
    VkDescriptorSetLayoutSupport* pSupport) {
    vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkDescriptorPool* pDescriptorPool) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pDescriptorPool || pCreateInfo->maxSets == 0 ||
        (pCreateInfo->poolSizeCount != 0 && !pCreateInfo->pPoolSizes)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pDescriptorPool = VK_NULL_HANDLE;
    auto* pool = new (std::nothrow) nv::NullDescriptorPool();
    if (!pool) return VK_ERROR_OUT_OF_HOST_MEMORY;
    pool->device = dev;
    pool->flags = pCreateInfo->flags;
    pool->maxSets = pCreateInfo->maxSets;
    pool->capacities.assign(pCreateInfo->pPoolSizes, pCreateInfo->pPoolSizes + pCreateInfo->poolSizeCount);
    pool->handle = reinterpret_cast<VkDescriptorPool>(pool);
    *pDescriptorPool = pool->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* pool = ToDescriptorPool(descriptorPool);
    if (!dev || !pool || pool->device != dev) return;
    ResetDescriptorPool(pool);
    pool->magic = 0;
    delete pool;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags) {
    auto* dev = ToDevice(device);
    auto* pool = ToDescriptorPool(descriptorPool);
    if (!dev || !pool || pool->device != dev) return VK_ERROR_INITIALIZATION_FAILED;
    ResetDescriptorPool(pool);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo,
    VkDescriptorSet* pDescriptorSets) {
    auto* dev = ToDevice(device);
    auto* pool = pAllocateInfo ? ToDescriptorPool(pAllocateInfo->descriptorPool) : nullptr;
    if (!dev || !pool || pool->device != dev || !pDescriptorSets ||
        (pAllocateInfo->descriptorSetCount != 0 && !pAllocateInfo->pSetLayouts) ||
        pAllocateInfo->descriptorSetCount > pool->maxSets - std::min<uint32_t>(pool->sets.size(), pool->maxSets)) {
        return VK_ERROR_OUT_OF_POOL_MEMORY;
    }
    for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i) pDescriptorSets[i] = VK_NULL_HANDLE;
    const auto* variableCounts = static_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo*>(nullptr);
    for (auto* next = reinterpret_cast<const VkBaseInStructure*>(pAllocateInfo->pNext); next; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO) {
            variableCounts = reinterpret_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo*>(next);
        }
    }

    for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; ++i) {
        auto* layout = ToSetLayout(pAllocateInfo->pSetLayouts[i]);
        if (!layout || layout->device != dev) {
            vkFreeDescriptorSets(device, pool->handle, i, pDescriptorSets);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        auto* set = new (std::nothrow) nv::NullDescriptorSet();
        if (!set) {
            vkFreeDescriptorSets(device, pool->handle, i, pDescriptorSets);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        set->device = dev;
        set->pool = pool;
        set->layout = layout;
        for (size_t bindingIndex = 0; bindingIndex < layout->bindings.size(); ++bindingIndex) {
            const auto& binding = layout->bindings[bindingIndex];
            nv::DescriptorBindingState state;
            state.binding = binding.binding;
            state.type = binding.descriptorType;
            uint32_t count = binding.descriptorCount;
            if (variableCounts && i < variableCounts->descriptorSetCount &&
                bindingIndex + 1 == layout->bindings.size()) {
                count = std::min(count, variableCounts->pDescriptorCounts[i]);
            }
            state.slots.resize(count);
            for (auto& slot : state.slots) slot.type = binding.descriptorType;
            set->bindings.push_back(std::move(state));
        }
        set->handle = reinterpret_cast<VkDescriptorSet>(set);
        pool->sets.push_back(set);
        pDescriptorSets[i] = set->handle;
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(
    VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount,
    const VkDescriptorSet* pDescriptorSets) {
    auto* dev = ToDevice(device);
    auto* pool = ToDescriptorPool(descriptorPool);
    if (!dev || !pool || pool->device != dev || (descriptorSetCount != 0 && !pDescriptorSets)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    for (uint32_t i = 0; i < descriptorSetCount; ++i) {
        auto* set = ToDescriptorSet(pDescriptorSets[i]);
        if (!set || set->pool != pool) continue;
        auto iterator = std::find(pool->sets.begin(), pool->sets.end(), set);
        if (iterator != pool->sets.end()) pool->sets.erase(iterator);
        DeleteDescriptorSet(set);
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites,
    uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    auto* dev = ToDevice(device);
    if (!dev || (descriptorWriteCount != 0 && !pDescriptorWrites) ||
        (descriptorCopyCount != 0 && !pDescriptorCopies)) return;

    for (uint32_t writeIndex = 0; writeIndex < descriptorWriteCount; ++writeIndex) {
        const auto& write = pDescriptorWrites[writeIndex];
        auto* set = ToDescriptorSet(write.dstSet);
        auto* binding = set ? FindBinding(set, write.dstBinding) : nullptr;
        if (!set || set->device != dev || !binding || binding->type != write.descriptorType ||
            write.dstArrayElement > binding->slots.size() ||
            write.descriptorCount > binding->slots.size() - write.dstArrayElement) continue;
        for (uint32_t i = 0; i < write.descriptorCount; ++i) {
            auto& slot = binding->slots[write.dstArrayElement + i];
            switch (write.descriptorType) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                if (write.pImageInfo) {
                    slot.sampler = write.pImageInfo[i].sampler;
                    slot.imageView = write.pImageInfo[i].imageView;
                    slot.imageLayout = write.pImageInfo[i].imageLayout;
                }
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                if (write.pTexelBufferView) slot.bufferView = write.pTexelBufferView[i];
                break;
            default:
                if (write.pBufferInfo) {
                    slot.buffer = write.pBufferInfo[i].buffer;
                    slot.offset = write.pBufferInfo[i].offset;
                    slot.range = write.pBufferInfo[i].range;
                }
                break;
            }
        }
    }
    for (uint32_t copyIndex = 0; copyIndex < descriptorCopyCount; ++copyIndex) {
        const auto& copy = pDescriptorCopies[copyIndex];
        auto* sourceSet = ToDescriptorSet(copy.srcSet);
        auto* destinationSet = ToDescriptorSet(copy.dstSet);
        auto* source = sourceSet ? FindBinding(sourceSet, copy.srcBinding) : nullptr;
        auto* destination = destinationSet ? FindBinding(destinationSet, copy.dstBinding) : nullptr;
        if (!source || !destination || source->type != destination->type ||
            copy.srcArrayElement > source->slots.size() || copy.dstArrayElement > destination->slots.size() ||
            copy.descriptorCount > source->slots.size() - copy.srcArrayElement ||
            copy.descriptorCount > destination->slots.size() - copy.dstArrayElement) continue;
        std::copy_n(source->slots.begin() + copy.srcArrayElement, copy.descriptorCount,
                    destination->slots.begin() + copy.dstArrayElement);
    }
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkPipelineLayout* pPipelineLayout) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pPipelineLayout ||
        (pCreateInfo->setLayoutCount != 0 && !pCreateInfo->pSetLayouts) ||
        (pCreateInfo->pushConstantRangeCount != 0 && !pCreateInfo->pPushConstantRanges)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pPipelineLayout = VK_NULL_HANDLE;
    auto* layout = new (std::nothrow) nv::NullPipelineLayout();
    if (!layout) return VK_ERROR_OUT_OF_HOST_MEMORY;
    layout->device = dev;
    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i) {
        auto* setLayout = ToSetLayout(pCreateInfo->pSetLayouts[i]);
        if (!setLayout || setLayout->device != dev) {
            delete layout;
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        layout->setLayouts.push_back(setLayout);
    }
    layout->pushConstantRanges.assign(pCreateInfo->pPushConstantRanges,
                                      pCreateInfo->pPushConstantRanges + pCreateInfo->pushConstantRangeCount);
    layout->handle = reinterpret_cast<VkPipelineLayout>(layout);
    *pPipelineLayout = layout->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* layout = ToPipelineLayout(pipelineLayout);
    if (!dev || !layout || layout->device != dev) return;
    layout->magic = 0;
    delete layout;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineCache(
    VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkPipelineCache* pPipelineCache) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pPipelineCache ||
        (pCreateInfo->initialDataSize != 0 && !pCreateInfo->pInitialData)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pPipelineCache = VK_NULL_HANDLE;
    auto* cache = new (std::nothrow) nv::NullPipelineCache();
    if (!cache) return VK_ERROR_OUT_OF_HOST_MEMORY;
    cache->device = dev;
    const auto* bytes = static_cast<const uint8_t*>(pCreateInfo->pInitialData);
    if (bytes) cache->data.assign(bytes, bytes + pCreateInfo->initialDataSize);
    cache->handle = reinterpret_cast<VkPipelineCache>(cache);
    *pPipelineCache = cache->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineCache(
    VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* cache = ToPipelineCache(pipelineCache);
    if (!dev || !cache || cache->device != dev) return;
    cache->magic = 0;
    delete cache;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineCacheData(
    VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    auto* dev = ToDevice(device);
    auto* cache = ToPipelineCache(pipelineCache);
    if (!dev || !cache || cache->device != dev || !pDataSize) return VK_ERROR_INITIALIZATION_FAILED;
    if (!pData) {
        *pDataSize = cache->data.size();
        return VK_SUCCESS;
    }
    const size_t copied = std::min(*pDataSize, cache->data.size());
    if (copied) std::memcpy(pData, cache->data.data(), copied);
    *pDataSize = copied;
    return copied < cache->data.size() ? VK_INCOMPLETE : VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMergePipelineCaches(
    VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount,
    const VkPipelineCache* pSrcCaches) {
    auto* dev = ToDevice(device);
    auto* destination = ToPipelineCache(dstCache);
    if (!dev || !destination || destination->device != dev || (srcCacheCount != 0 && !pSrcCaches)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    for (uint32_t i = 0; i < srcCacheCount; ++i) {
        auto* source = ToPipelineCache(pSrcCaches[i]);
        if (!source || source->device != dev) return VK_ERROR_INITIALIZATION_FAILED;
        destination->data.insert(destination->data.end(), source->data.begin(), source->data.end());
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice device, VkPipelineCache, uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
    auto* dev = ToDevice(device);
    if (!dev || (createInfoCount != 0 && (!pCreateInfos || !pPipelines))) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < createInfoCount; ++i) pPipelines[i] = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < createInfoCount; ++i) {
        VkResult result = CreatePipeline(dev, VK_PIPELINE_BIND_POINT_GRAPHICS, pCreateInfos[i].layout, &pPipelines[i]);
        if (result != VK_SUCCESS) {
            for (uint32_t created = 0; created < i; ++created) vkDestroyPipeline(device, pPipelines[created], nullptr);
            return result;
        }
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(
    VkDevice device, VkPipelineCache, uint32_t createInfoCount,
    const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
    auto* dev = ToDevice(device);
    if (!dev || (createInfoCount != 0 && (!pCreateInfos || !pPipelines))) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < createInfoCount; ++i) pPipelines[i] = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < createInfoCount; ++i) {
        VkResult result = CreatePipeline(dev, VK_PIPELINE_BIND_POINT_COMPUTE, pCreateInfos[i].layout, &pPipelines[i]);
        if (result != VK_SUCCESS) {
            for (uint32_t created = 0; created < i; ++created) vkDestroyPipeline(device, pPipelines[created], nullptr);
            return result;
        }
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(
    VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = nv::ValidateHandle<nv::NullPipeline>(pipeline, nv::kMagicPipeline);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(
    VkDevice device, const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkRenderPass* pRenderPass) {
    return CreateRenderPassObject(device, pCreateInfo, pRenderPass);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2(
    VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo,
    const VkAllocationCallbacks*, VkRenderPass* pRenderPass) {
    return CreateRenderPassObject(device, pCreateInfo, pRenderPass);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2KHR(
    VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo,
    const VkAllocationCallbacks* allocator, VkRenderPass* pRenderPass) {
    return vkCreateRenderPass2(device, pCreateInfo, allocator, pRenderPass);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(
    VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = ToRenderPass(renderPass);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(
    VkDevice device, const VkFramebufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkFramebuffer* pFramebuffer) {
    auto* dev = ToDevice(device);
    auto* renderPass = pCreateInfo ? ToRenderPass(pCreateInfo->renderPass) : nullptr;
    if (!dev || !renderPass || renderPass->device != dev || !pFramebuffer ||
        pCreateInfo->width == 0 || pCreateInfo->height == 0 || pCreateInfo->layers == 0 ||
        (pCreateInfo->attachmentCount != 0 && !pCreateInfo->pAttachments)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pFramebuffer = VK_NULL_HANDLE;
    auto* framebuffer = new (std::nothrow) nv::NullFramebuffer();
    if (!framebuffer) return VK_ERROR_OUT_OF_HOST_MEMORY;
    framebuffer->device = dev;
    framebuffer->renderPass = renderPass;
    framebuffer->width = pCreateInfo->width;
    framebuffer->height = pCreateInfo->height;
    framebuffer->layers = pCreateInfo->layers;
    for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i) {
        auto* view = ToImageView(pCreateInfo->pAttachments[i]);
        if (!view || view->device != dev) {
            delete framebuffer;
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        framebuffer->attachments.push_back(view);
    }
    framebuffer->handle = reinterpret_cast<VkFramebuffer>(framebuffer);
    *pFramebuffer = framebuffer->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(
    VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = nv::ValidateHandle<nv::NullFramebuffer>(framebuffer, nv::kMagicFramebuffer);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetRenderAreaGranularity(
    VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    auto* dev = ToDevice(device);
    auto* object = ToRenderPass(renderPass);
    if (dev && object && object->device == dev && pGranularity) *pGranularity = {1, 1};
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(
    VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents) {
    (void)RecordingCommandBuffer(commandBuffer);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    (void)RecordingCommandBuffer(commandBuffer);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2(
    VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo*, const VkSubpassBeginInfo*) {
    (void)RecordingCommandBuffer(commandBuffer);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2KHR(
    VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* begin, const VkSubpassBeginInfo* subpass) {
    vkCmdBeginRenderPass2(commandBuffer, begin, subpass);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2(
    VkCommandBuffer commandBuffer, const VkSubpassEndInfo*) {
    (void)RecordingCommandBuffer(commandBuffer);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2KHR(
    VkCommandBuffer commandBuffer, const VkSubpassEndInfo* subpass) {
    vkCmdEndRenderPass2(commandBuffer, subpass);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(
    VkCommandBuffer commandBuffer, VkSubpassContents) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2(
    VkCommandBuffer commandBuffer, const VkSubpassBeginInfo*, const VkSubpassEndInfo*) {
    (void)RecordingCommandBuffer(commandBuffer);
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2KHR(
    VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* begin, const VkSubpassEndInfo* end) {
    vkCmdNextSubpass2(commandBuffer, begin, end);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint, VkPipeline) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t,
    uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(
    VkCommandBuffer commandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*) {
    (void)RecordingCommandBuffer(commandBuffer);
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(
    VkCommandBuffer commandBuffer, VkBuffer, VkDeviceSize, VkIndexType) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDraw(
    VkCommandBuffer commandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer commandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t) {
    (void)RecordingCommandBuffer(commandBuffer);
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(
    VkCommandBuffer commandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(
    VkCommandBuffer commandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(
    VkCommandBuffer commandBuffer, uint32_t, uint32_t, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(
    VkCommandBuffer commandBuffer, uint32_t, uint32_t, const VkViewport*) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(
    VkCommandBuffer commandBuffer, uint32_t, uint32_t, const VkRect2D*) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBias(
    VkCommandBuffer commandBuffer, float, float, float) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilCompareMask(
    VkCommandBuffer commandBuffer, VkStencilFaceFlags, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilWriteMask(
    VkCommandBuffer commandBuffer, VkStencilFaceFlags, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilReference(
    VkCommandBuffer commandBuffer, VkStencilFaceFlags, uint32_t) { (void)RecordingCommandBuffer(commandBuffer); }
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(
    VkCommandBuffer commandBuffer, VkPipelineLayout, VkShaderStageFlags,
    uint32_t, uint32_t, const void*) { (void)RecordingCommandBuffer(commandBuffer); }
