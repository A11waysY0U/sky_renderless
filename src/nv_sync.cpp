// CPU synchronization primitives for the immediate-execution queue.

#include "nv_objects.h"

#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <new>

namespace {

std::mutex g_fenceMutex;
std::condition_variable g_fenceCondition;

nv::NullDevice* ToDevice(VkDevice handle) {
    return nv::ValidateHandle<nv::NullDevice>(handle, nv::kMagicDevice);
}

nv::NullFence* ToFence(VkFence handle) {
    return nv::ValidateHandle<nv::NullFence>(handle, nv::kMagicFence);
}

nv::NullSemaphore* ToSemaphore(VkSemaphore handle) {
    return nv::ValidateHandle<nv::NullSemaphore>(handle, nv::kMagicSemaphore);
}

} // namespace

void nv::SignalFence(NullFence* fence) {
    if (!fence) return;
    fence->signaled.store(true, std::memory_order_release);
    g_fenceCondition.notify_all();
}

void nv::SignalSemaphore(NullSemaphore* semaphore) {
    if (semaphore) semaphore->signaled.store(true, std::memory_order_release);
}

bool nv::ConsumeSemaphore(NullSemaphore* semaphore) {
    if (!semaphore) return false;
    bool expected = true;
    return semaphore->signaled.compare_exchange_strong(
        expected, false, std::memory_order_acq_rel, std::memory_order_acquire);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(
    VkDevice device, const VkFenceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkFence* pFence) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pFence) return VK_ERROR_INITIALIZATION_FAILED;
    *pFence = VK_NULL_HANDLE;
    auto* fence = new (std::nothrow) nv::NullFence();
    if (!fence) return VK_ERROR_OUT_OF_HOST_MEMORY;
    fence->device = dev;
    fence->signaled.store((pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT) != 0);
    fence->handle = reinterpret_cast<VkFence>(fence);
    *pFence = fence->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyFence(
    VkDevice device, VkFence fence, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = ToFence(fence);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(
    VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    auto* dev = ToDevice(device);
    if (!dev || (fenceCount != 0 && !pFences)) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < fenceCount; ++i) {
        auto* fence = ToFence(pFences[i]);
        if (!fence || fence->device != dev) return VK_ERROR_INITIALIZATION_FAILED;
        fence->signaled.store(false, std::memory_order_release);
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice device, VkFence fence) {
    auto* dev = ToDevice(device);
    auto* object = ToFence(fence);
    if (!dev || !object || object->device != dev) return VK_ERROR_DEVICE_LOST;
    return object->signaled.load(std::memory_order_acquire) ? VK_SUCCESS : VK_NOT_READY;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(
    VkDevice device, uint32_t fenceCount, const VkFence* pFences,
    VkBool32 waitAll, uint64_t timeout) {
    auto* dev = ToDevice(device);
    if (!dev || fenceCount == 0 || !pFences) return VK_ERROR_INITIALIZATION_FAILED;

    for (uint32_t i = 0; i < fenceCount; ++i) {
        auto* fence = ToFence(pFences[i]);
        if (!fence || fence->device != dev) return VK_ERROR_INITIALIZATION_FAILED;
    }
    const auto satisfied = [&]() {
        bool any = false;
        for (uint32_t i = 0; i < fenceCount; ++i) {
            const bool signaled = ToFence(pFences[i])->signaled.load(std::memory_order_acquire);
            if (waitAll && !signaled) return false;
            any = any || signaled;
        }
        return waitAll || any;
    };
    if (satisfied()) return VK_SUCCESS;
    if (timeout == 0) return VK_TIMEOUT;

    std::unique_lock<std::mutex> lock(g_fenceMutex);
    if (timeout == std::numeric_limits<uint64_t>::max()) {
        g_fenceCondition.wait(lock, satisfied);
        return VK_SUCCESS;
    }
    const uint64_t maximumNanos = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    const auto duration = std::chrono::nanoseconds(static_cast<int64_t>(std::min(timeout, maximumNanos)));
    return g_fenceCondition.wait_for(lock, duration, satisfied) ? VK_SUCCESS : VK_TIMEOUT;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(
    VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*, VkSemaphore* pSemaphore) {
    auto* dev = ToDevice(device);
    if (!dev || !pCreateInfo || !pSemaphore) return VK_ERROR_INITIALIZATION_FAILED;
    *pSemaphore = VK_NULL_HANDLE;
    auto* semaphore = new (std::nothrow) nv::NullSemaphore();
    if (!semaphore) return VK_ERROR_OUT_OF_HOST_MEMORY;
    semaphore->device = dev;
    semaphore->handle = reinterpret_cast<VkSemaphore>(semaphore);
    *pSemaphore = semaphore->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(
    VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = ToSemaphore(semaphore);
    if (!dev || !object || object->device != dev) return;
    object->magic = 0;
    delete object;
}
