// Win32 surface and CPU-backed null swapchain.

#include "nv_objects.h"

#include <algorithm>
#include <limits>
#include <new>
#include <windows.h>

namespace {

constexpr VkSurfaceFormatKHR kSurfaceFormats[] = {
    {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
};

constexpr VkPresentModeKHR kPresentModes[] = {
    VK_PRESENT_MODE_FIFO_KHR,
    VK_PRESENT_MODE_MAILBOX_KHR,
    VK_PRESENT_MODE_IMMEDIATE_KHR,
};

nv::NullInstance* ToInstance(VkInstance handle) {
    return nv::ValidateHandle<nv::NullInstance>(handle, nv::kMagicInstance);
}

nv::NullPhysicalDevice* ToPhysicalDevice(VkPhysicalDevice handle) {
    return nv::ValidateHandle<nv::NullPhysicalDevice>(handle, nv::kMagicPhysicalDevice);
}

nv::NullDevice* ToDevice(VkDevice handle) {
    return nv::ValidateHandle<nv::NullDevice>(handle, nv::kMagicDevice);
}

nv::NullQueue* ToQueue(VkQueue handle) {
    return nv::ValidateHandle<nv::NullQueue>(handle, nv::kMagicQueue);
}

nv::NullSurface* ToSurface(VkSurfaceKHR handle) {
    return nv::ValidateHandle<nv::NullSurface>(handle, nv::kMagicSurface);
}

nv::NullSwapchain* ToSwapchain(VkSwapchainKHR handle) {
    return nv::ValidateHandle<nv::NullSwapchain>(handle, nv::kMagicSwapchain);
}

VkExtent2D SurfaceExtent(const nv::NullSurface* surface) {
    RECT client{};
    HWND window = static_cast<HWND>(surface->window);
    if (window && GetClientRect(window, &client)) {
        return {
            static_cast<uint32_t>(std::max<LONG>(client.right - client.left, 1)),
            static_cast<uint32_t>(std::max<LONG>(client.bottom - client.top, 1)),
        };
    }
    return {1280, 720};
}

VkSurfaceCapabilitiesKHR SurfaceCapabilities(const nv::NullSurface* surface) {
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 2;
    capabilities.maxImageCount = 8;
    capabilities.currentExtent = SurfaceExtent(surface);
    capabilities.minImageExtent = {1, 1};
    capabilities.maxImageExtent = {16384, 16384};
    capabilities.maxImageArrayLayers = 1;
    capabilities.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR |
                                           VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT;
    return capabilities;
}

template <typename T, size_t Count>
VkResult Enumerate(const T (&values)[Count], uint32_t* count, T* output) {
    if (!count) return VK_ERROR_INITIALIZATION_FAILED;
    if (!output) {
        *count = static_cast<uint32_t>(Count);
        return VK_SUCCESS;
    }
    const uint32_t copied = std::min(*count, static_cast<uint32_t>(Count));
    for (uint32_t i = 0; i < copied; ++i) output[i] = values[i];
    *count = copied;
    return copied < Count ? VK_INCOMPLETE : VK_SUCCESS;
}

void DestroySwapchain(nv::NullSwapchain* swapchain) {
    for (auto* image : swapchain->images) {
        if (image) {
            image->magic = 0;
            delete image;
        }
    }
    for (auto* memory : swapchain->imageMemory) {
        if (memory) {
            memory->magic = 0;
            delete memory;
        }
    }
    swapchain->images.clear();
    swapchain->imageMemory.clear();
    swapchain->magic = 0;
    delete swapchain;
}

VkResult SignalAcquireObjects(nv::NullDevice* device, VkSemaphore semaphoreHandle, VkFence fenceHandle) {
    nv::NullSemaphore* semaphore = nullptr;
    nv::NullFence* fence = nullptr;
    if (semaphoreHandle != VK_NULL_HANDLE) {
        semaphore = nv::ValidateHandle<nv::NullSemaphore>(semaphoreHandle, nv::kMagicSemaphore);
        if (!semaphore || semaphore->device != device) return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (fenceHandle != VK_NULL_HANDLE) {
        fence = nv::ValidateHandle<nv::NullFence>(fenceHandle, nv::kMagicFence);
        if (!fence || fence->device != device) return VK_ERROR_INITIALIZATION_FAILED;
    }
    nv::SignalSemaphore(semaphore);
    nv::SignalFence(fence);
    return VK_SUCCESS;
}

} // namespace

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(
    VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*, VkSurfaceKHR* pSurface) {
    auto* inst = ToInstance(instance);
    if (!inst || !pCreateInfo || !pSurface || !pCreateInfo->hwnd) return VK_ERROR_INITIALIZATION_FAILED;
    *pSurface = VK_NULL_HANDLE;
    auto* surface = new (std::nothrow) nv::NullSurface();
    if (!surface) return VK_ERROR_OUT_OF_HOST_MEMORY;
    surface->instance = inst;
    surface->window = pCreateInfo->hwnd;
    surface->handle = reinterpret_cast<VkSurfaceKHR>(surface);
    *pSurface = surface->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceWin32PresentationSupportKHR(
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex) {
    return ToPhysicalDevice(physicalDevice) && queueFamilyIndex == 0 ? VK_TRUE : VK_FALSE;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(
    VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks*) {
    auto* inst = ToInstance(instance);
    auto* object = ToSurface(surface);
    if (!inst || !object || object->instance != inst) return;
    object->magic = 0;
    delete object;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
    VkSurfaceKHR surface, VkBool32* pSupported) {
    auto* pd = ToPhysicalDevice(physicalDevice);
    auto* object = ToSurface(surface);
    if (!pd || !object || object->instance != pd->instance || !pSupported) return VK_ERROR_SURFACE_LOST_KHR;
    *pSupported = queueFamilyIndex == 0 ? VK_TRUE : VK_FALSE;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
    auto* pd = ToPhysicalDevice(physicalDevice);
    auto* object = ToSurface(surface);
    if (!pd || !object || object->instance != pd->instance || !pSurfaceCapabilities) {
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    *pSurfaceCapabilities = SurfaceCapabilities(object);
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
    auto* pd = ToPhysicalDevice(physicalDevice);
    auto* object = ToSurface(surface);
    if (!pd || !object || object->instance != pd->instance) return VK_ERROR_SURFACE_LOST_KHR;
    return Enumerate(kSurfaceFormats, pSurfaceFormatCount, pSurfaceFormats);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) {
    auto* pd = ToPhysicalDevice(physicalDevice);
    auto* object = ToSurface(surface);
    if (!pd || !object || object->instance != pd->instance) return VK_ERROR_SURFACE_LOST_KHR;
    return Enumerate(kPresentModes, pPresentModeCount, pPresentModes);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    VkSurfaceCapabilities2KHR* pSurfaceCapabilities) {
    if (!pSurfaceInfo || !pSurfaceCapabilities) return VK_ERROR_INITIALIZATION_FAILED;
    return vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, pSurfaceInfo->surface, &pSurfaceCapabilities->surfaceCapabilities);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats) {
    if (!pSurfaceInfo || !pSurfaceFormatCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (!pSurfaceFormats) return vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, pSurfaceInfo->surface, pSurfaceFormatCount, nullptr);

    uint32_t capacity = *pSurfaceFormatCount;
    VkSurfaceFormatKHR formats[sizeof(kSurfaceFormats) / sizeof(kSurfaceFormats[0])]{};
    uint32_t count = capacity;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, pSurfaceInfo->surface, &count, formats);
    for (uint32_t i = 0; i < count; ++i) pSurfaceFormats[i].surfaceFormat = formats[i];
    *pSurfaceFormatCount = count;
    return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*, VkSwapchainKHR* pSwapchain) {
    auto* dev = ToDevice(device);
    auto* surface = pCreateInfo ? ToSurface(pCreateInfo->surface) : nullptr;
    auto* pd = dev ? ToPhysicalDevice(dev->physDevice) : nullptr;
    if (!dev || !pd || !surface || surface->instance != pd->instance || !pSwapchain ||
        pCreateInfo->minImageCount == 0 || pCreateInfo->imageExtent.width == 0 ||
        pCreateInfo->imageExtent.height == 0 || pCreateInfo->imageArrayLayers != 1) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pSwapchain = VK_NULL_HANDLE;
    const uint32_t imageCount = std::clamp(pCreateInfo->minImageCount, 2u, 8u);
    auto* swapchain = new (std::nothrow) nv::NullSwapchain();
    if (!swapchain) return VK_ERROR_OUT_OF_HOST_MEMORY;
    swapchain->device = dev;
    swapchain->surface = surface;
    swapchain->format = pCreateInfo->imageFormat;
    swapchain->colorSpace = pCreateInfo->imageColorSpace;
    swapchain->extent = pCreateInfo->imageExtent;
    swapchain->usage = pCreateInfo->imageUsage;
    swapchain->presentMode = pCreateInfo->presentMode;
    swapchain->handle = reinterpret_cast<VkSwapchainKHR>(swapchain);

    const uint64_t pixels = static_cast<uint64_t>(swapchain->extent.width) * swapchain->extent.height;
    const uint64_t imageBytes = pixels * 4;
    if (imageBytes == 0 || imageBytes > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        delete swapchain;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    for (uint32_t i = 0; i < imageCount; ++i) {
        auto* memory = new (std::nothrow) nv::NullDeviceMemory();
        auto* image = new (std::nothrow) nv::NullImage();
        if (!memory || !image) {
            delete memory;
            delete image;
            DestroySwapchain(swapchain);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memory->device = dev;
        memory->allocationSize = imageBytes;
        memory->memoryTypeIndex = 0;
        memory->bytes.reset(new (std::nothrow) uint8_t[static_cast<size_t>(imageBytes)]{});
        if (!memory->bytes) {
            delete memory;
            delete image;
            DestroySwapchain(swapchain);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memory->handle = reinterpret_cast<VkDeviceMemory>(memory);
        image->device = dev;
        image->format = swapchain->format;
        image->extent = {swapchain->extent.width, swapchain->extent.height, 1};
        image->usage = swapchain->usage;
        image->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image->memory = memory;
        image->handle = reinterpret_cast<VkImage>(image);
        swapchain->imageMemory.push_back(memory);
        swapchain->images.push_back(image);
    }
    *pSwapchain = swapchain->handle;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks*) {
    auto* dev = ToDevice(device);
    auto* object = ToSwapchain(swapchain);
    if (!dev || !object || object->device != dev) return;
    DestroySwapchain(object);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(
    VkDevice device, VkSwapchainKHR swapchain,
    uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    auto* dev = ToDevice(device);
    auto* object = ToSwapchain(swapchain);
    if (!dev || !object || object->device != dev || !pSwapchainImageCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (!pSwapchainImages) {
        *pSwapchainImageCount = static_cast<uint32_t>(object->images.size());
        return VK_SUCCESS;
    }
    const uint32_t copied = std::min(*pSwapchainImageCount, static_cast<uint32_t>(object->images.size()));
    for (uint32_t i = 0; i < copied; ++i) pSwapchainImages[i] = object->images[i]->handle;
    *pSwapchainImageCount = copied;
    return copied < object->images.size() ? VK_INCOMPLETE : VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice device, VkSwapchainKHR swapchain, uint64_t,
    VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    auto* dev = ToDevice(device);
    auto* object = ToSwapchain(swapchain);
    if (!dev || !object || object->device != dev || !pImageIndex || object->images.empty()) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    VkResult result = SignalAcquireObjects(dev, semaphore, fence);
    if (result != VK_SUCCESS) return result;
    *pImageIndex = object->nextImage.fetch_add(1, std::memory_order_relaxed) %
                   static_cast<uint32_t>(object->images.size());
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHR(
    VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    if (!pAcquireInfo) return VK_ERROR_INITIALIZATION_FAILED;
    return vkAcquireNextImageKHR(device, pAcquireInfo->swapchain, pAcquireInfo->timeout,
                                 pAcquireInfo->semaphore, pAcquireInfo->fence, pImageIndex);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    auto* object = ToQueue(queue);
    if (!object || !pPresentInfo ||
        (pPresentInfo->waitSemaphoreCount != 0 && !pPresentInfo->pWaitSemaphores) ||
        (pPresentInfo->swapchainCount != 0 && (!pPresentInfo->pSwapchains || !pPresentInfo->pImageIndices))) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    for (uint32_t i = 0; i < pPresentInfo->waitSemaphoreCount; ++i) {
        auto* semaphore = nv::ValidateHandle<nv::NullSemaphore>(
            pPresentInfo->pWaitSemaphores[i], nv::kMagicSemaphore);
        if (!semaphore || semaphore->device != object->device || !nv::ConsumeSemaphore(semaphore)) {
            return VK_ERROR_DEVICE_LOST;
        }
    }
    VkResult overall = VK_SUCCESS;
    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
        auto* swapchain = ToSwapchain(pPresentInfo->pSwapchains[i]);
        VkResult result = swapchain && swapchain->device == object->device &&
                          pPresentInfo->pImageIndices[i] < swapchain->images.size()
            ? VK_SUCCESS : VK_ERROR_OUT_OF_DATE_KHR;
        if (pPresentInfo->pResults) pPresentInfo->pResults[i] = result;
        if (result != VK_SUCCESS) overall = result;
    }
    return overall;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainStatusKHR(
    VkDevice device, VkSwapchainKHR swapchain) {
    auto* dev = ToDevice(device);
    auto* object = ToSwapchain(swapchain);
    return dev && object && object->device == dev ? VK_SUCCESS : VK_ERROR_OUT_OF_DATE_KHR;
}
