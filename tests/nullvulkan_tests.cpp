#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>

namespace {

int Fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

uint32_t FindHostVisibleMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) != 0 &&
            (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace

int main() {
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "nullvulkan_tests";
    application.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &application;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS || !instance) {
        return Fail("instance creation");
    }

    uint32_t physicalDeviceCount = 1;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    if (vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevice) != VK_SUCCESS ||
        physicalDeviceCount != 1 || !physicalDevice) {
        return Fail("physical device enumeration");
    }

    VkFormatProperties3 formatProperties3{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3};
    VkFormatProperties2 formatProperties2{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
    formatProperties2.pNext = &formatProperties3;
    vkGetPhysicalDeviceFormatProperties2(
        physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties2);
    if ((formatProperties2.formatProperties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0 ||
        (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT) == 0) {
        return Fail("color format properties");
    }
    VkFormatProperties depthFormatProperties{};
    vkGetPhysicalDeviceFormatProperties(
        physicalDevice, VK_FORMAT_D32_SFLOAT, &depthFormatProperties);
    if ((depthFormatProperties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) {
        return Fail("depth format properties");
    }
    VkImageFormatProperties imageFormatProperties{};
    if (vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, VK_FORMAT_BC7_UNORM_BLOCK, VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0,
            &imageFormatProperties) != VK_SUCCESS ||
        imageFormatProperties.maxExtent.width == 0 || imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT) {
        return Fail("compressed image format properties");
    }

    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    features11.pNext = &features12;
    features12.pNext = &features13;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &features11;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
    if (features11.pNext != &features12 || features12.pNext != &features13 ||
        features12.bufferDeviceAddress != VK_TRUE) {
        return Fail("feature pNext chain and bufferDeviceAddress");
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS || !device) {
        return Fail("device creation");
    }
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, 0, 0, &queue);
    if (!queue) return Fail("device queue");

    auto createBuffer = reinterpret_cast<PFN_vkCreateBuffer>(vkGetDeviceProcAddr(device, "vkCreateBuffer"));
    auto mapMemory = reinterpret_cast<PFN_vkMapMemory>(vkGetDeviceProcAddr(device, "vkMapMemory"));
    if (!createBuffer || !mapMemory) return Fail("VMA entry point lookup");

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 4096;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (createBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS || !buffer) {
        return Fail("buffer creation");
    }

    VkMemoryRequirements bufferRequirements{};
    vkGetBufferMemoryRequirements(device, buffer, &bufferRequirements);
    const uint32_t hostMemoryType = FindHostVisibleMemoryType(physicalDevice, bufferRequirements.memoryTypeBits);
    if (hostMemoryType == UINT32_MAX || bufferRequirements.size < bufferInfo.size) {
        return Fail("buffer memory requirements");
    }

    VkMemoryAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocationInfo.allocationSize = bufferRequirements.size;
    allocationInfo.memoryTypeIndex = hostMemoryType;
    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocationInfo, nullptr, &bufferMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        return Fail("buffer memory allocation and binding");
    }

    void* mapped = nullptr;
    if (mapMemory(device, bufferMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS || !mapped) {
        return Fail("memory mapping");
    }
    std::memset(mapped, 0x5a, static_cast<size_t>(bufferInfo.size));
    vkUnmapMemory(device, bufferMemory);
    if (mapMemory(device, bufferMemory, 128, 16, 0, &mapped) != VK_SUCCESS ||
        static_cast<unsigned char*>(mapped)[0] != 0x5a) {
        return Fail("mapped storage persistence");
    }
    vkUnmapMemory(device, bufferMemory);

    auto mapMemory2 = reinterpret_cast<PFN_vkMapMemory2KHR>(
        vkGetDeviceProcAddr(device, "vkMapMemory2KHR"));
    auto unmapMemory2 = reinterpret_cast<PFN_vkUnmapMemory2KHR>(
        vkGetDeviceProcAddr(device, "vkUnmapMemory2KHR"));
    VkMemoryMapInfoKHR mapInfo{VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR};
    mapInfo.memory = bufferMemory;
    mapInfo.offset = 256;
    mapInfo.size = 16;
    if (!mapMemory2 || !unmapMemory2 || mapMemory2(device, &mapInfo, &mapped) != VK_SUCCESS ||
        static_cast<unsigned char*>(mapped)[0] != 0x5a) {
        return Fail("vkMapMemory2KHR alias");
    }
    VkMemoryUnmapInfoKHR unmapInfo{VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR};
    unmapInfo.memory = bufferMemory;
    if (unmapMemory2(device, &unmapInfo) != VK_SUCCESS) return Fail("vkUnmapMemory2KHR alias");

    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addressInfo.buffer = buffer;
    if (vkGetBufferDeviceAddress(device, &addressInfo) == 0) return Fail("fake buffer device address");

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {8, 8, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS || !image) {
        return Fail("image creation");
    }
    VkMemoryRequirements imageRequirements{};
    vkGetImageMemoryRequirements(device, image, &imageRequirements);
    allocationInfo.allocationSize = imageRequirements.size;
    allocationInfo.memoryTypeIndex = 0;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocationInfo, nullptr, &imageMemory) != VK_SUCCESS ||
        vkBindImageMemory(device, image, imageMemory, 0) != VK_SUCCESS) {
        return Fail("image memory allocation and binding");
    }

    VkBuffer destinationBuffer = VK_NULL_HANDLE;
    VkDeviceMemory destinationMemory = VK_NULL_HANDLE;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &destinationBuffer) != VK_SUCCESS) {
        return Fail("destination buffer creation");
    }
    allocationInfo.allocationSize = bufferRequirements.size;
    allocationInfo.memoryTypeIndex = hostMemoryType;
    if (vkAllocateMemory(device, &allocationInfo, nullptr, &destinationMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, destinationBuffer, destinationMemory, 0) != VK_SUCCESS) {
        return Fail("destination buffer binding");
    }

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS || !commandPool) {
        return Fail("command pool creation");
    }
    VkCommandBufferAllocateInfo commandAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandAllocateInfo.commandPool = commandPool;
    commandAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &commandAllocateInfo, &commandBuffer) != VK_SUCCESS || !commandBuffer) {
        return Fail("command buffer allocation");
    }
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) return Fail("begin command buffer");

    vkCmdFillBuffer(commandBuffer, destinationBuffer, 0, VK_WHOLE_SIZE, 0x11223344u);
    const uint32_t updatedWords[] = {0xdeadbeefu, 0x12345678u};
    vkCmdUpdateBuffer(commandBuffer, destinationBuffer, 512, sizeof(updatedWords), updatedWords);
    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = 128;
    bufferCopy.size = 256;
    vkCmdCopyBuffer(commandBuffer, buffer, destinationBuffer, 1, &bufferCopy);

    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
    VkBufferImageCopy imageCopy{};
    imageCopy.bufferOffset = 0;
    imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.imageSubresource.layerCount = 1;
    imageCopy.imageExtent = {8, 8, 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
    imageCopy.bufferOffset = 1024;
    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           destinationBuffer, 1, &imageCopy);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) return Fail("end command buffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    if (vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(queue) != VK_SUCCESS) {
        return Fail("CPU queue submission");
    }

    mapped = nullptr;
    if (vkMapMemory(device, destinationMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
        return Fail("destination readback map");
    }
    const auto* destinationBytes = static_cast<const unsigned char*>(mapped);
    uint32_t fillWord = 0;
    uint32_t updateWord = 0;
    std::memcpy(&fillWord, destinationBytes, sizeof(fillWord));
    std::memcpy(&updateWord, destinationBytes + 512, sizeof(updateWord));
    if (fillWord != 0x11223344u || updateWord != updatedWords[0] ||
        destinationBytes[128] != 0x5a || destinationBytes[383] != 0x5a ||
        destinationBytes[1024] != 0x5a || destinationBytes[1279] != 0x5a) {
        return Fail("submitted copy/update/fill/image data");
    }
    vkUnmapMemory(device, destinationMemory);

    VkBufferCreateInfo shortBufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    shortBufferInfo.size = 384;
    shortBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    shortBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer shortBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(device, &shortBufferInfo, nullptr, &shortBuffer) != VK_SUCCESS) {
        return Fail("short staging buffer creation");
    }
    VkMemoryRequirements shortBufferRequirements{};
    vkGetBufferMemoryRequirements(device, shortBuffer, &shortBufferRequirements);
    allocationInfo.allocationSize = shortBufferRequirements.size;
    allocationInfo.memoryTypeIndex = hostMemoryType;
    VkDeviceMemory shortBufferMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocationInfo, nullptr, &shortBufferMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, shortBuffer, shortBufferMemory, 0) != VK_SUCCESS ||
        vkMapMemory(device, shortBufferMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
        return Fail("short staging buffer allocation");
    }
    std::memset(mapped, 0x7c, 384);
    vkUnmapMemory(device, shortBufferMemory);

    VkImageCreateInfo r16ImageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    r16ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    r16ImageInfo.format = VK_FORMAT_R16_SFLOAT;
    r16ImageInfo.extent = {16, 13, 1};
    r16ImageInfo.mipLevels = 1;
    r16ImageInfo.arrayLayers = 1;
    r16ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    r16ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    r16ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    r16ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkImage r16Image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &r16ImageInfo, nullptr, &r16Image) != VK_SUCCESS) {
        return Fail("R16 image creation");
    }
    VkMemoryRequirements r16Requirements{};
    vkGetImageMemoryRequirements(device, r16Image, &r16Requirements);
    allocationInfo.allocationSize = r16Requirements.size;
    allocationInfo.memoryTypeIndex = 0;
    VkDeviceMemory r16Memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocationInfo, nullptr, &r16Memory) != VK_SUCCESS ||
        vkBindImageMemory(device, r16Image, r16Memory, 0) != VK_SUCCESS) {
        return Fail("R16 image allocation");
    }

    if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS ||
        vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        return Fail("short staging command begin");
    }
    VkBufferImageCopy shortCopy{};
    shortCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    shortCopy.imageSubresource.layerCount = 1;
    shortCopy.imageExtent = {16, 13, 1};
    vkCmdCopyBufferToImage(commandBuffer, shortBuffer, r16Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &shortCopy);
    shortCopy.bufferOffset = 2048;
    vkCmdCopyImageToBuffer(commandBuffer, r16Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           destinationBuffer, 1, &shortCopy);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS ||
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
        return Fail("short staging submission");
    }
    if (vkMapMemory(device, destinationMemory, 2048, 416, 0, &mapped) != VK_SUCCESS) {
        return Fail("short staging readback map");
    }
    const auto* shortReadback = static_cast<const unsigned char*>(mapped);
    for (size_t i = 0; i < 384; ++i) {
        if (shortReadback[i] != 0x7c) return Fail("short staging preserved bytes");
    }
    for (size_t i = 384; i < 416; ++i) {
        if (shortReadback[i] != 0) return Fail("short staging synthesized tail");
    }
    vkUnmapMemory(device, destinationMemory);
    vkDestroyImage(device, r16Image, nullptr);
    vkFreeMemory(device, r16Memory, nullptr);
    vkDestroyBuffer(device, shortBuffer, nullptr);
    vkFreeMemory(device, shortBufferMemory, nullptr);

    VkFence acquireFence = VK_NULL_HANDLE;
    VkFence submitFence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(device, &fenceInfo, nullptr, &acquireFence) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &submitFence) != VK_SUCCESS ||
        vkGetFenceStatus(device, submitFence) != VK_NOT_READY) {
        return Fail("fence creation and initial state");
    }
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished) != VK_SUCCESS) {
        return Fail("semaphore creation");
    }

    VkWin32SurfaceCreateInfoKHR surfaceInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surfaceInfo.hinstance = GetModuleHandleW(nullptr);
    surfaceInfo.hwnd = GetDesktopWindow();
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS || !surface) {
        return Fail("Win32 surface creation");
    }
    VkBool32 presentSupported = VK_FALSE;
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    uint32_t surfaceFormatCount = 0;
    if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, 0, surface, &presentSupported) != VK_SUCCESS ||
        !presentSupported ||
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS ||
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr) != VK_SUCCESS ||
        surfaceFormatCount == 0) {
        return Fail("surface queries");
    }

    VkSwapchainCreateInfoKHR swapchainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = 3;
    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = {64, 64};
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain) != VK_SUCCESS || !swapchain) {
        return Fail("swapchain creation");
    }
    uint32_t swapchainImageCount = 0;
    if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr) != VK_SUCCESS ||
        swapchainImageCount != 3) {
        return Fail("swapchain image enumeration");
    }
    VkImage swapchainImages[3]{};
    if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages) != VK_SUCCESS ||
        !swapchainImages[0] || !swapchainImages[1] || !swapchainImages[2]) {
        return Fail("swapchain image handles");
    }

    uint32_t acquiredIndex = UINT32_MAX;
    if (vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailable, acquireFence,
                              &acquiredIndex) != VK_SUCCESS || acquiredIndex >= swapchainImageCount ||
        vkWaitForFences(device, 1, &acquireFence, VK_TRUE, 0) != VK_SUCCESS ||
        vkResetFences(device, 1, &acquireFence) != VK_SUCCESS ||
        vkGetFenceStatus(device, acquireFence) != VK_NOT_READY ||
        vkWaitForFences(device, 1, &acquireFence, VK_TRUE, 0) != VK_TIMEOUT) {
        return Fail("swapchain acquire and fence completion");
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo presentSubmit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    presentSubmit.waitSemaphoreCount = 1;
    presentSubmit.pWaitSemaphores = &imageAvailable;
    presentSubmit.pWaitDstStageMask = &waitStage;
    presentSubmit.signalSemaphoreCount = 1;
    presentSubmit.pSignalSemaphores = &renderFinished;
    if (vkQueueSubmit(queue, 1, &presentSubmit, submitFence) != VK_SUCCESS ||
        vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return Fail("semaphore submit and fence signal");
    }
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &acquiredIndex;
    if (vkQueuePresentKHR(queue, &presentInfo) != VK_SUCCESS) return Fail("null present");

    uint32_t nextAcquiredIndex = UINT32_MAX;
    if (vkAcquireNextImageKHR(device, swapchain, 0, VK_NULL_HANDLE, acquireFence,
                              &nextAcquiredIndex) != VK_SUCCESS ||
        nextAcquiredIndex == acquiredIndex ||
        vkWaitForFences(device, 1, &acquireFence, VK_TRUE, 0) != VK_SUCCESS) {
        return Fail("rotating swapchain acquire");
    }

    VkImageViewCreateInfo imageViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewInfo.image = swapchainImages[0];
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) != VK_SUCCESS || !imageView) {
        return Fail("image view creation");
    }
    VkBufferViewCreateInfo bufferViewInfo{VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
    bufferViewInfo.buffer = buffer;
    bufferViewInfo.format = VK_FORMAT_R32_UINT;
    bufferViewInfo.range = VK_WHOLE_SIZE;
    VkBufferView bufferView = VK_NULL_HANDLE;
    if (vkCreateBufferView(device, &bufferViewInfo, nullptr, &bufferView) != VK_SUCCESS || !bufferView) {
        return Fail("buffer view creation");
    }
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS || !sampler) {
        return Fail("sampler creation");
    }
    const uint32_t shaderCode[] = {0x07230203u, 0x00010000u, 0u, 1u, 0u};
    VkShaderModuleCreateInfo shaderInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderInfo.codeSize = sizeof(shaderCode);
    shaderInfo.pCode = shaderCode;
    VkShaderModule shader = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shader) != VK_SUCCESS || !shader) {
        return Fail("shader module creation");
    }

    VkDescriptorSetLayoutBinding descriptorBindings[3]{};
    descriptorBindings[0].binding = 0;
    descriptorBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorBindings[0].descriptorCount = 1;
    descriptorBindings[0].stageFlags = VK_SHADER_STAGE_ALL;
    descriptorBindings[1].binding = 1;
    descriptorBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorBindings[1].descriptorCount = 1;
    descriptorBindings[1].stageFlags = VK_SHADER_STAGE_ALL;
    descriptorBindings[2].binding = 2;
    descriptorBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    descriptorBindings[2].descriptorCount = 1;
    descriptorBindings[2].stageFlags = VK_SHADER_STAGE_ALL;
    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = 3;
    setLayoutInfo.pBindings = descriptorBindings;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &setLayout) != VK_SUCCESS || !setLayout) {
        return Fail("descriptor set layout creation");
    }
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2},
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.maxSets = 2;
    descriptorPoolInfo.poolSizeCount = 3;
    descriptorPoolInfo.pPoolSizes = poolSizes;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        return Fail("descriptor pool creation");
    }
    VkDescriptorSetAllocateInfo setAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAllocateInfo.descriptorPool = descriptorPool;
    setAllocateInfo.descriptorSetCount = 1;
    setAllocateInfo.pSetLayouts = &setLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &setAllocateInfo, &descriptorSet) != VK_SUCCESS || !descriptorSet) {
        return Fail("descriptor set allocation");
    }
    VkDescriptorBufferInfo descriptorBufferInfo{buffer, 0, VK_WHOLE_SIZE};
    VkDescriptorImageInfo descriptorImageInfo{sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet descriptorWrites[3]{};
    for (auto& write : descriptorWrites) write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].pImageInfo = &descriptorImageInfo;
    descriptorWrites[2].dstSet = descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    descriptorWrites[2].pTexelBufferView = &bufferView;
    vkUpdateDescriptorSets(device, 3, descriptorWrites, 0, nullptr);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &setLayout;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS || !pipelineLayout) {
        return Fail("pipeline layout creation");
    }
    VkPipelineCacheCreateInfo cacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    if (vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache) != VK_SUCCESS || !pipelineCache) {
        return Fail("pipeline cache creation");
    }

    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS || !renderPass) {
        return Fail("render pass creation");
    }
    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &imageView;
    framebufferInfo.width = 64;
    framebufferInfo.height = 64;
    framebufferInfo.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS || !framebuffer) {
        return Fail("framebuffer creation");
    }

    VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader;
    stageInfo.pName = "main";
    VkComputePipelineCreateInfo computeInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computeInfo.stage = stageInfo;
    computeInfo.layout = pipelineLayout;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(device, pipelineCache, 1, &computeInfo, nullptr, &computePipeline) != VK_SUCCESS ||
        !computePipeline) {
        return Fail("compute pipeline creation");
    }
    VkGraphicsPipelineCreateInfo graphicsInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    graphicsInfo.layout = pipelineLayout;
    graphicsInfo.renderPass = renderPass;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsInfo, nullptr, &graphicsPipeline) != VK_SUCCESS ||
        !graphicsPipeline) {
        return Fail("graphics pipeline creation");
    }

    if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS ||
        vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        return Fail("render command begin");
    }
    VkRenderPassBeginInfo renderBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderBegin.renderPass = renderPass;
    renderBegin.framebuffer = framebuffer;
    renderBegin.renderArea.extent = {64, 64};
    vkCmdBeginRenderPass(commandBuffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdDispatch(commandBuffer, 1, 1, 1);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS ||
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
        return Fail("render and compute no-op submission");
    }

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyBufferView(device, bufferView, nullptr);
    vkDestroyImageView(device, imageView, nullptr);

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroySemaphore(device, renderFinished, nullptr);
    vkDestroySemaphore(device, imageAvailable, nullptr);
    vkDestroyFence(device, submitFence, nullptr);
    vkDestroyFence(device, acquireFence, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyBuffer(device, destinationBuffer, nullptr);
    vkFreeMemory(device, destinationMemory, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, imageMemory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, bufferMemory, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    std::puts("nullvulkan API tests passed");
    return 0;
}
