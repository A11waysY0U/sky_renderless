# gen_vulkan.ps1
# Generates, from third_party Vulkan-Headers vulkan_core.h:
#   - src/generated/nv_generated.cpp : one uniform-signature stub per vk function (M1) + name lookup table
#   - src/generated/vulkan-1.def      : export list (game resolves every function via GetProcAddress)
#
# M1 stub strategy: every vk symbol the game may resolve MUST be exported AND defined.
# Stubs use a single uniform signature `VkResult vkFoo()` — safe on x64 (callee ignores extra
# args in registers/stack, return in RAX). Real implementations live in hand-written files;
# add their names to $Real so the generator skips stubbing them (the real file defines them).
#
# Run:  powershell -ExecutionPolicy Bypass -File tools\gen_vulkan.ps1

param(
    [string]$Root = (Join-Path $PSScriptRoot "..")
)

$headerPaths = @(
    (Join-Path $Root "third_party\vulkan_headers\include\vulkan\vulkan_core.h"),
    (Join-Path $Root "third_party\Vulkan-Headers\include\vulkan\vulkan_win32.h")
)
$outCpp     = Join-Path $Root "src\generated\nv_generated.cpp"
$outDef     = Join-Path $Root "src\generated\vulkan-1.def"
$outDir     = Split-Path $outCpp
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

foreach ($headerPath in $headerPaths) {
    if (-not (Test-Path $headerPath)) { throw "Vulkan header not found: $headerPath" }
}

# ---- collect function names ----
$names = [System.Collections.Generic.HashSet[string]]::new()
foreach ($headerPath in $headerPaths) {
    foreach ($line in Get-Content $headerPath) {
        if ($line -match '\bVKAPI_CALL\s+(vk[A-Za-z0-9_]+)\s*\(') {
            [void]$names.Add($Matches[1])
        }
    }
}
$all = @($names | Sort-Object)
Write-Host "collected $($all.Count) vk function names"

# ---- functions implemented for real (blocklist) ----
$Real = [System.Collections.Generic.HashSet[string]]::new()
@(
    'vkGetInstanceProcAddr','vkGetDeviceProcAddr',
    'vkCreateInstance','vkDestroyInstance',
    'vkEnumerateInstanceVersion','vkEnumerateInstanceExtensionProperties','vkEnumerateInstanceLayerProperties',
    'vkEnumeratePhysicalDevices','vkGetPhysicalDeviceProperties','vkGetPhysicalDeviceProperties2',
    'vkGetPhysicalDeviceFeatures','vkGetPhysicalDeviceFeatures2',
    'vkGetPhysicalDeviceFeatures2KHR',
    'vkGetPhysicalDeviceQueueFamilyProperties','vkGetPhysicalDeviceQueueFamilyProperties2',
    'vkGetPhysicalDeviceQueueFamilyProperties2KHR',
    'vkGetPhysicalDeviceMemoryProperties','vkGetPhysicalDeviceMemoryProperties2',
    'vkGetPhysicalDeviceMemoryProperties2KHR',
    'vkGetPhysicalDeviceProperties2KHR',
    'vkGetPhysicalDeviceFormatProperties','vkGetPhysicalDeviceFormatProperties2',
    'vkGetPhysicalDeviceFormatProperties2KHR','vkGetPhysicalDeviceImageFormatProperties',
    'vkGetPhysicalDeviceImageFormatProperties2','vkGetPhysicalDeviceImageFormatProperties2KHR',
    'vkGetPhysicalDeviceSparseImageFormatProperties','vkGetPhysicalDeviceSparseImageFormatProperties2',
    'vkGetPhysicalDeviceSparseImageFormatProperties2KHR',
    'vkEnumerateDeviceExtensionProperties','vkEnumerateDeviceLayerProperties',
    'vkCreateDevice','vkDestroyDevice','vkGetDeviceQueue','vkGetDeviceQueue2',
    'vkAllocateMemory','vkFreeMemory','vkMapMemory','vkUnmapMemory','vkMapMemory2KHR','vkUnmapMemory2KHR',
    'vkFlushMappedMemoryRanges','vkInvalidateMappedMemoryRanges','vkGetDeviceMemoryCommitment',
    'vkCreateBuffer','vkDestroyBuffer','vkGetBufferMemoryRequirements','vkGetBufferMemoryRequirements2',
    'vkGetBufferMemoryRequirements2KHR','vkGetDeviceBufferMemoryRequirements','vkGetDeviceBufferMemoryRequirementsKHR',
    'vkBindBufferMemory','vkBindBufferMemory2','vkBindBufferMemory2KHR',
    'vkCreateImage','vkDestroyImage','vkGetImageMemoryRequirements','vkGetImageMemoryRequirements2',
    'vkGetImageMemoryRequirements2KHR','vkGetDeviceImageMemoryRequirements','vkGetDeviceImageMemoryRequirementsKHR',
    'vkBindImageMemory','vkBindImageMemory2','vkBindImageMemory2KHR',
    'vkGetBufferDeviceAddress','vkGetBufferDeviceAddressKHR','vkGetBufferOpaqueCaptureAddress',
    'vkGetBufferOpaqueCaptureAddressKHR','vkGetDeviceMemoryOpaqueCaptureAddress',
    'vkGetDeviceMemoryOpaqueCaptureAddressKHR',
    'vkCreateCommandPool','vkDestroyCommandPool','vkResetCommandPool',
    'vkAllocateCommandBuffers','vkFreeCommandBuffers','vkBeginCommandBuffer',
    'vkEndCommandBuffer','vkResetCommandBuffer',
    'vkCmdCopyBuffer','vkCmdUpdateBuffer','vkCmdFillBuffer',
    'vkCmdBlitImage','vkCmdBlitImage2','vkCmdBlitImage2KHR',
    'vkCmdCopyBufferToImage','vkCmdCopyImageToBuffer','vkCmdCopyImage','vkCmdPipelineBarrier',
    'vkQueueSubmit','vkQueueWaitIdle','vkDeviceWaitIdle',
    'vkCreateFence','vkDestroyFence','vkResetFences','vkGetFenceStatus','vkWaitForFences',
    'vkCreateSemaphore','vkDestroySemaphore',
    'vkCreateWin32SurfaceKHR','vkGetPhysicalDeviceWin32PresentationSupportKHR','vkDestroySurfaceKHR',
    'vkGetPhysicalDeviceSurfaceSupportKHR','vkGetPhysicalDeviceSurfaceCapabilitiesKHR',
    'vkGetPhysicalDeviceSurfaceFormatsKHR','vkGetPhysicalDeviceSurfacePresentModesKHR',
    'vkGetPhysicalDeviceSurfaceCapabilities2KHR','vkGetPhysicalDeviceSurfaceFormats2KHR',
    'vkCreateSwapchainKHR','vkDestroySwapchainKHR','vkGetSwapchainImagesKHR',
    'vkAcquireNextImageKHR','vkAcquireNextImage2KHR','vkQueuePresentKHR','vkGetSwapchainStatusKHR',
    'vkCreateSampler','vkDestroySampler','vkCreateSamplerYcbcrConversion','vkDestroySamplerYcbcrConversion',
    'vkCreateSamplerYcbcrConversionKHR','vkDestroySamplerYcbcrConversionKHR',
    'vkCreateImageView','vkDestroyImageView','vkCreateBufferView','vkDestroyBufferView',
    'vkCreateShaderModule','vkDestroyShaderModule',
    'vkCreateDescriptorSetLayout','vkDestroyDescriptorSetLayout','vkGetDescriptorSetLayoutSupport',
    'vkGetDescriptorSetLayoutSupportKHR',
    'vkCreateDescriptorPool','vkDestroyDescriptorPool','vkResetDescriptorPool',
    'vkAllocateDescriptorSets','vkFreeDescriptorSets','vkUpdateDescriptorSets',
    'vkCreatePipelineLayout','vkDestroyPipelineLayout','vkCreatePipelineCache','vkDestroyPipelineCache',
    'vkGetPipelineCacheData','vkMergePipelineCaches','vkCreateGraphicsPipelines',
    'vkCreateComputePipelines','vkDestroyPipeline',
    'vkCreateRenderPass','vkCreateRenderPass2','vkCreateRenderPass2KHR','vkDestroyRenderPass',
    'vkCreateFramebuffer','vkDestroyFramebuffer','vkGetRenderAreaGranularity',
    'vkCmdBeginRenderPass','vkCmdEndRenderPass','vkCmdBeginRenderPass2','vkCmdBeginRenderPass2KHR',
    'vkCmdEndRenderPass2','vkCmdEndRenderPass2KHR','vkCmdNextSubpass','vkCmdNextSubpass2','vkCmdNextSubpass2KHR',
    'vkCmdBindPipeline','vkCmdBindDescriptorSets','vkCmdBindVertexBuffers','vkCmdBindIndexBuffer',
    'vkCmdDraw','vkCmdDrawIndexed','vkCmdDrawIndirect','vkCmdDrawIndexedIndirect','vkCmdDispatch',
    'vkCmdSetViewport','vkCmdSetScissor','vkCmdSetDepthBias','vkCmdSetStencilCompareMask',
    'vkCmdSetStencilWriteMask','vkCmdSetStencilReference','vkCmdPushConstants'
) | ForEach-Object { [void]$Real.Add($_) }

$stubs = @($all | Where-Object { -not $Real.Contains($_) })

# ---- generate cpp ----
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// AUTO-GENERATED by tools\gen_vulkan.ps1 -- do not edit.")
[void]$sb.AppendLine("#define VK_NO_PROTOTYPES")
[void]$sb.AppendLine("#include <vulkan/vulkan.h>")
[void]$sb.AppendLine("#include <cstring>")
[void]$sb.AppendLine('#include "nv_log.h"')
[void]$sb.AppendLine("")
[void]$sb.AppendLine("extern `"C`" {")
[void]$sb.AppendLine("// ---- M1 uniform stubs (replaced by real impls in later milestones) ----")
foreach ($n in $stubs) {
    [void]$sb.AppendLine("VKAPI_ATTR VkResult VKAPI_CALL $n() { NV_STUB(`"$n`"); return VK_SUCCESS; }")
}
[void]$sb.AppendLine("")
[void]$sb.AppendLine("// ---- real implementations (defined in hand-written translation units) ----")
foreach ($n in $all) {
    [void]$sb.AppendLine("VKAPI_ATTR VkResult VKAPI_CALL $n(); // forward decl (real signature lives in impl file)")
}
[void]$sb.AppendLine("} // extern `"C`"")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("// ---- name lookup table ----")
[void]$sb.AppendLine("namespace nv {")
[void]$sb.AppendLine("struct NameEntry { const char* name; PFN_vkVoidFunction fn; };")
[void]$sb.AppendLine("static const NameEntry kNameTable[] = {")
foreach ($n in $all) {
    [void]$sb.AppendLine("    { `"$n`", (PFN_vkVoidFunction)&$n },")
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("static const size_t kNameCount = sizeof(kNameTable) / sizeof(kNameTable[0]);")
[void]$sb.AppendLine("PFN_vkVoidFunction LookupName(const char* name) {")
[void]$sb.AppendLine("    if (!name) return nullptr;")
[void]$sb.AppendLine("    for (size_t i = 0; i < kNameCount; ++i) { if (strcmp(kNameTable[i].name, name) == 0) return kNameTable[i].fn; }")
[void]$sb.AppendLine("    return nullptr;")
[void]$sb.AppendLine("}")
[void]$sb.AppendLine("} // namespace nv")
Set-Content -Path $outCpp -Value $sb.ToString() -Encoding utf8

# ---- generate .def ----
$sb2 = New-Object System.Text.StringBuilder
[void]$sb2.AppendLine("LIBRARY vulkan-1")
[void]$sb2.AppendLine("EXPORTS")
[void]$sb2.AppendLine("    vkGetInstanceProcAddr")
[void]$sb2.AppendLine("    vkGetDeviceProcAddr")
[void]$sb2.AppendLine("    vk_icdGetInstanceProcAddr")
[void]$sb2.AppendLine("    vk_icdGetPhysicalDeviceProcAddr")
[void]$sb2.AppendLine("    vk_icdNegotiateLoaderICDInterfaceVersion")
foreach ($n in $all) {
    [void]$sb2.AppendLine("    $n")
}
Set-Content -Path $outDef -Value $sb2.ToString() -Encoding ascii

Write-Host "generated:"
Write-Host "  $outCpp"
Write-Host "  $outDef"
Write-Host "  stubs=$($stubs.Count) real=$($Real.Count) total=$($all.Count)"
