// nv_instance.cpp
// Real implementations of instance/device creation and enumeration.
// Every function here is in the blocklist (not auto-stubbed).

#include "nv_log.h"
#include "nv_objects.h"
#include <vulkan/vulkan.h>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static nv::NullInstance* ToInstance(VkInstance inst) {
    return nv::ValidateHandle<nv::NullInstance>(inst, nv::kMagicInstance);
}
static nv::NullPhysicalDevice* ToPhysDevice(VkPhysicalDevice pd) {
    return nv::ValidateHandle<nv::NullPhysicalDevice>(pd, nv::kMagicPhysicalDevice);
}
static nv::NullDevice* ToDevice(VkDevice dev) {
    return nv::ValidateHandle<nv::NullDevice>(dev, nv::kMagicDevice);
}
static nv::NullQueue* ToQueue(VkQueue q) {
    return nv::ValidateHandle<nv::NullQueue>(q, nv::kMagicQueue);
}

// ---------------------------------------------------------------------------
// vkCreateInstance
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance)
{
    NV_LOG("vkCreateInstance called");
    if (!pCreateInfo || !pInstance) return VK_ERROR_INITIALIZATION_FAILED;

    // Create instance object
    auto* inst = new nv::NullInstance();
    if (pCreateInfo->pApplicationInfo) {
        inst->apiVersion = pCreateInfo->pApplicationInfo->apiVersion;
        strncpy_s(inst->appName, sizeof(inst->appName),
                  pCreateInfo->pApplicationInfo->pApplicationName ? pCreateInfo->pApplicationInfo->pApplicationName : "",
                  _TRUNCATE);
        strncpy_s(inst->engineName, sizeof(inst->engineName),
                  pCreateInfo->pApplicationInfo->pEngineName ? pCreateInfo->pApplicationInfo->pEngineName : "",
                  _TRUNCATE);
    }
    inst->handle = reinterpret_cast<VkInstance>(inst);

    // Create physical device
    auto* pd = new nv::NullPhysicalDevice();
    pd->handle = reinterpret_cast<VkPhysicalDevice>(pd);
    pd->instance = inst;
    nv::InitPhysicalDevice(pd);
    inst->physDevice = pd;

    *pInstance = inst->handle;
    NV_LOG("  Instance=%p apiVersion=0x%x", inst, inst->apiVersion);
    return VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkDestroyInstance
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance instance,
    const VkAllocationCallbacks* pAllocator)
{
    auto* inst = ToInstance(instance);
    if (!inst) return;
    NV_LOG("vkDestroyInstance %p", inst);
    if (inst->physDevice) delete inst->physDevice;
    delete inst;
}

// ---------------------------------------------------------------------------
// vkEnumerateInstanceVersion
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceVersion(
    uint32_t* pApiVersion)
{
    if (pApiVersion) *pApiVersion = VK_API_VERSION_1_3;
    NV_LOG("vkEnumerateInstanceVersion -> 1.3");
    return VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkEnumerateInstanceExtensionProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties)
{
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (pLayerName && pLayerName[0] != '\0') {
        // No layer extensions
        if (pPropertyCount) *pPropertyCount = 0;
        return VK_SUCCESS;
    }
    // We need a physical device object to get the list. Create a temporary one if needed.
    static nv::NullPhysicalDevice s_tempPD;
    static bool s_init = false;
    if (!s_init) {
        nv::InitPhysicalDevice(&s_tempPD);
        s_init = true;
    }
    const auto& list = s_tempPD.instanceExtensions;
    if (pProperties == nullptr) {
        *pPropertyCount = (uint32_t)list.size();
        return VK_SUCCESS;
    }
    uint32_t capacity = *pPropertyCount;
    uint32_t copy = (capacity < list.size()) ? capacity : (uint32_t)list.size();
    for (uint32_t i = 0; i < copy; ++i) {
        pProperties[i] = list[i];
    }
    *pPropertyCount = copy;
    return (copy < list.size()) ? VK_INCOMPLETE : VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkEnumerateInstanceLayerProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t* pPropertyCount,
    VkLayerProperties* pProperties)
{
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkEnumeratePhysicalDevices
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance instance,
    uint32_t* pPhysicalDeviceCount,
    VkPhysicalDevice* pPhysicalDevices)
{
    auto* inst = ToInstance(instance);
    if (!inst || !pPhysicalDeviceCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (pPhysicalDevices == nullptr) {
        *pPhysicalDeviceCount = 1;
        return VK_SUCCESS;
    }
    if (*pPhysicalDeviceCount < 1) {
        *pPhysicalDeviceCount = 0;
        return VK_INCOMPLETE;
    }
    *pPhysicalDeviceCount = 1;
    pPhysicalDevices[0] = inst->physDevice->handle;
    NV_LOG("vkEnumeratePhysicalDevices -> %p", inst->physDevice);
    return VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties* pProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pProperties) return;
    *pProperties = pd->props;
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceProperties2
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties2* pProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pProperties) return;
    pProperties->properties = pd->props;
    // Walk pNext chain for ID properties, etc. — just fill properties and return.
    VkBaseOutStructure* cur = (VkBaseOutStructure*)pProperties->pNext;
    while (cur) {
        // We could fill VkPhysicalDeviceIDProperties, VkPhysicalDeviceMaintenance3Properties, etc.
        // For M1, leave them default-initialized (caller must have set sType).
        cur = cur->pNext;
    }
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2KHR(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties2* pProperties)
{
    vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceFeatures
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures* pFeatures)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pFeatures) return;
    *pFeatures = pd->features.features;
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceFeatures2
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures2* pFeatures)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pFeatures) return;
    pFeatures->features = pd->features.features;

    // Walk pNext: fill VkPhysicalDeviceVulkan11/12/13Features and any other known structs.
    VkBaseOutStructure* cur = (VkBaseOutStructure*)pFeatures->pNext;
    while (cur) {
        VkBaseOutStructure* next = cur->pNext;
        switch (cur->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES: {
            auto* out = reinterpret_cast<VkPhysicalDeviceVulkan11Features*>(cur);
            void* callerNext = out->pNext;
            *out = pd->vk11Features;
            out->pNext = callerNext;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES: {
            auto* out = reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(cur);
            void* callerNext = out->pNext;
            *out = pd->vk12Features;
            out->pNext = callerNext;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: {
            auto* out = reinterpret_cast<VkPhysicalDeviceVulkan13Features*>(cur);
            void* callerNext = out->pNext;
            *out = pd->vk13Features;
            out->pNext = callerNext;
            break;
        }
        default:
            // Unknown extension feature struct — leave as-is (zeroed by caller).
            break;
        }
        cur = next;
    }
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2KHR(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures2* pFeatures)
{
    vkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceQueueFamilyProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties* pQueueFamilyProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pQueueFamilyPropertyCount) return;
    if (pQueueFamilyProperties == nullptr) {
        *pQueueFamilyPropertyCount = pd->queueFamilyCount;
        return;
    }
    uint32_t count = *pQueueFamilyPropertyCount;
    *pQueueFamilyPropertyCount = pd->queueFamilyCount;
    uint32_t copy = (count < pd->queueFamilyCount) ? count : pd->queueFamilyCount;
    for (uint32_t i = 0; i < copy; ++i) {
        pQueueFamilyProperties[i] = pd->queueFamilies[i];
    }
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceQueueFamilyProperties2
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice physicalDevice,
    uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2* pQueueFamilyProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pQueueFamilyPropertyCount) return;
    if (pQueueFamilyProperties == nullptr) {
        *pQueueFamilyPropertyCount = pd->queueFamilyCount;
        return;
    }
    uint32_t count = *pQueueFamilyPropertyCount;
    *pQueueFamilyPropertyCount = pd->queueFamilyCount;
    uint32_t copy = (count < pd->queueFamilyCount) ? count : pd->queueFamilyCount;
    for (uint32_t i = 0; i < copy; ++i) {
        pQueueFamilyProperties[i].queueFamilyProperties = pd->queueFamilies[i];
    }
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2KHR(
    VkPhysicalDevice physicalDevice,
    uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2* pQueueFamilyProperties)
{
    vkGetPhysicalDeviceQueueFamilyProperties2(
        physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceMemoryProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pMemoryProperties) return;
    *pMemoryProperties = pd->memProps;
}

// ---------------------------------------------------------------------------
// vkGetPhysicalDeviceMemoryProperties2
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties2* pMemoryProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pMemoryProperties) return;
    pMemoryProperties->memoryProperties = pd->memProps;
    for (auto* cur = reinterpret_cast<VkBaseOutStructure*>(pMemoryProperties->pNext);
         cur; cur = cur->pNext) {
        if (cur->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT) {
            auto* budget = reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(cur);
            for (uint32_t i = 0; i < pd->memProps.memoryHeapCount; ++i) {
                budget->heapBudget[i] = pd->memProps.memoryHeaps[i].size;
                budget->heapUsage[i] = 0;
            }
        }
    }
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2KHR(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties2* pMemoryProperties)
{
    vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
}

// ---------------------------------------------------------------------------
// vkEnumerateDeviceExtensionProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties)
{
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (pLayerName && pLayerName[0] != '\0') {
        if (pPropertyCount) *pPropertyCount = 0;
        return VK_SUCCESS;
    }
    const auto& list = pd->deviceExtensions;
    if (pProperties == nullptr) {
        *pPropertyCount = (uint32_t)list.size();
        return VK_SUCCESS;
    }
    uint32_t capacity = *pPropertyCount;
    uint32_t copy = (capacity < list.size()) ? capacity : (uint32_t)list.size();
    for (uint32_t i = 0; i < copy; ++i) {
        pProperties[i] = list[i];
    }
    *pPropertyCount = copy;
    return (copy < list.size()) ? VK_INCOMPLETE : VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkEnumerateDeviceLayerProperties
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pPropertyCount,
    VkLayerProperties* pProperties)
{
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkCreateDevice
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    NV_LOG("vkCreateDevice called");
    auto* pd = ToPhysDevice(physicalDevice);
    if (!pd || !pCreateInfo || !pDevice) return VK_ERROR_INITIALIZATION_FAILED;

    // Validate queue creation
    if (pCreateInfo->queueCreateInfoCount == 0 || !pCreateInfo->pQueueCreateInfos) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create device
    auto* dev = new nv::NullDevice();
    dev->handle = reinterpret_cast<VkDevice>(dev);
    dev->physDevice = physicalDevice;
    dev->apiVersion = pd->props.apiVersion;

    // Create queues
    uint32_t totalQueues = 0;
    for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount && totalQueues < nv::NullDevice::kMaxQueues; ++i) {
        const auto& qci = pCreateInfo->pQueueCreateInfos[i];
        uint32_t count = qci.queueCount;
        if (count > nv::NullDevice::kMaxQueues - totalQueues) {
            count = nv::NullDevice::kMaxQueues - totalQueues;
        }
        for (uint32_t j = 0; j < count; ++j) {
            auto* q = new nv::NullQueue();
            q->handle = reinterpret_cast<VkQueue>(q);
            q->device = dev;
            q->familyIndex = qci.queueFamilyIndex;
            q->queueIndex = j;
            dev->queues[totalQueues++] = q;
        }
    }
    dev->queueCount = totalQueues;

    *pDevice = dev->handle;

    // Log enabled extensions
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        NV_LOG("  enabled extension: %s", pCreateInfo->ppEnabledExtensionNames[i]);
    }

    NV_LOG("  Device=%p queues=%u", dev, totalQueues);
    return VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// vkDestroyDevice
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice device,
    const VkAllocationCallbacks* pAllocator)
{
    auto* dev = ToDevice(device);
    if (!dev) return;
    NV_LOG("vkDestroyDevice %p", dev);
    for (uint32_t i = 0; i < dev->queueCount; ++i) {
        if (dev->queues[i]) delete dev->queues[i];
    }
    delete dev;
}

// ---------------------------------------------------------------------------
// vkGetDeviceQueue
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(
    VkDevice device,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* pQueue)
{
    auto* dev = ToDevice(device);
    if (!dev || !pQueue) { if (pQueue) *pQueue = nullptr; return; }
    for (uint32_t i = 0; i < dev->queueCount; ++i) {
        if (dev->queues[i] && dev->queues[i]->familyIndex == queueFamilyIndex &&
            dev->queues[i]->queueIndex == queueIndex) {
            *pQueue = dev->queues[i]->handle;
            return;
        }
    }
    *pQueue = nullptr;
    NV_LOG("[WARN] vkGetDeviceQueue(%u,%u) not found", queueFamilyIndex, queueIndex);
}

// ---------------------------------------------------------------------------
// vkGetDeviceQueue2
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue2(
    VkDevice device,
    const VkDeviceQueueInfo2* pQueueInfo,
    VkQueue* pQueue)
{
    if (!pQueueInfo || !pQueue) { if (pQueue) *pQueue = nullptr; return; }
    vkGetDeviceQueue(device, pQueueInfo->queueFamilyIndex, pQueueInfo->queueIndex, pQueue);
}
