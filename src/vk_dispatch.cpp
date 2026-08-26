// vk_dispatch.cpp
// Entry points for vkGetInstanceProcAddr, vkGetDeviceProcAddr, and ICD negotiator.
// These are the REAL implementations (not stubs) — they survive into all milestones.

#include "nv_log.h"
#include "nv_objects.h"
#include <vulkan/vulkan.h>
#include <cstring>

// Generated lookup: nv::LookupName(name) -> PFN_vkVoidFunction
namespace nv { PFN_vkVoidFunction LookupName(const char* name); }

// ---------------------------------------------------------------------------
// One-time init (called from the first dispatch entry)
// ---------------------------------------------------------------------------
static bool g_init = false;

static void EnsureInit() {
    if (g_init) return;
    nv::LogInit();
    g_init = true;
}

// ---------------------------------------------------------------------------
// vkGetInstanceProcAddr
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance instance, const char* pName)
{
    EnsureInit();
    if (!pName) return nullptr;
    // Forward to the generated name table.
    PFN_vkVoidFunction fn = nv::LookupName(pName);
    if (!fn) {
        NV_LOG("[DISPATCH] vkGetInstanceProcAddr('%s'): NOT FOUND", pName);
    }
    return fn;
}

// ---------------------------------------------------------------------------
// vkGetDeviceProcAddr
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice device, const char* pName)
{
    EnsureInit();
    if (!pName) return nullptr;
    PFN_vkVoidFunction fn = nv::LookupName(pName);
    if (!fn) {
        NV_LOG("[DISPATCH] vkGetDeviceProcAddr('%s'): NOT FOUND", pName);
    }
    return fn;
}

// ---------------------------------------------------------------------------
// ICD interface (these are used if the loader is the real vulkan-1.dll and
// discovers us as an ICD via VK_ICD_FILENAMES; harmless to export regardless)
// ---------------------------------------------------------------------------
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(
    VkInstance instance, const char* pName)
{
    return vkGetInstanceProcAddr(instance, pName);
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetPhysicalDeviceProcAddr(
    VkInstance instance, const char* pName)
{
    // Physical-device-only functions: vkGetPhysicalDeviceProperties, etc.
    // Our table includes them all; just delegate.
    return vkGetInstanceProcAddr(instance, pName);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(
    uint32_t* pSupportedVersion)
{
    EnsureInit();
    if (!pSupportedVersion) return VK_ERROR_INITIALIZATION_FAILED;
    // ICD interface version 5 supports all features we need.
    // Version 6 adds vk_icdGetPhysicalDeviceProcAddr.
    // Version 1 is the minimum.
    constexpr uint32_t kOurVersion = 6;
    if (*pSupportedVersion > kOurVersion) {
        *pSupportedVersion = kOurVersion;
    }
    NV_LOG("[ICD] vk_icdNegotiateLoaderICDInterfaceVersion: negotiated to %u", *pSupportedVersion);
    return VK_SUCCESS;
}