#pragma once
// nv_objects.h -- Null Vulkan Backend object types and handle mapping.
// All handles are opaque pointers to these structs.

#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace nv {

// ---------------------------------------------------------------------------
// Magic numbers for type-checking handles (debug)
// ---------------------------------------------------------------------------
enum : uint32_t {
    kMagicInstance       = 0x4E564953,  // "NVIS"
    kMagicPhysicalDevice = 0x4E565044,  // "NVPD"
    kMagicDevice         = 0x4E564445,  // "NVDE"
    kMagicQueue          = 0x4E565155,  // "NVQU"
    kMagicMemory         = 0x4E564D45,  // "NVME"
    kMagicBuffer         = 0x4E564255,  // "NVBU"
    kMagicImage          = 0x4E56494D,  // "NVIM"
    kMagicCommandPool    = 0x4E564350,  // "NVCP"
    kMagicCommandBuffer  = 0x4E564342,  // "NVCB"
    kMagicFence          = 0x4E56464E,  // "NVFN"
    kMagicSemaphore      = 0x4E56534D,  // "NVSM"
    kMagicSurface        = 0x4E565346,  // "NVSF"
    kMagicSwapchain      = 0x4E565357,  // "NVSW"
    kMagicSampler        = 0x4E565341,  // "NVSA"
    kMagicImageView      = 0x4E564956,  // "NVIV"
    kMagicBufferView     = 0x4E564256,  // "NVBV"
    kMagicShaderModule   = 0x4E565348,  // "NVSH"
    kMagicSetLayout      = 0x4E56444C,  // "NVDL"
    kMagicDescriptorPool = 0x4E564450,  // "NVDP"
    kMagicDescriptorSet  = 0x4E564453,  // "NVDS"
    kMagicPipelineLayout = 0x4E56504C,  // "NVPL"
    kMagicPipelineCache  = 0x4E565043,  // "NVPC"
    kMagicPipeline       = 0x4E565049,  // "NVPI"
    kMagicRenderPass     = 0x4E565250,  // "NVRP"
    kMagicFramebuffer    = 0x4E564642,  // "NVFB"
    kMagicYcbcr          = 0x4E565943,  // "NVYC"
};

// ---------------------------------------------------------------------------
// Instance
// ---------------------------------------------------------------------------
struct NullInstance {
    uint32_t                          magic      = kMagicInstance;
    uint32_t                          apiVersion = VK_API_VERSION_1_3; // requested
    VkInstance                        handle     = nullptr; // self-reference

    // The single physical device we expose.
    struct NullPhysicalDevice*         physDevice = nullptr;

    // Application info (copied)
    char                              appName[256]   = {};
    char                              engineName[256] = {};
};

// ---------------------------------------------------------------------------
// PhysicalDevice
// ---------------------------------------------------------------------------
struct NullPhysicalDevice {
    uint32_t                          magic       = kMagicPhysicalDevice;
    VkPhysicalDevice                   handle      = nullptr; // self-reference
    NullInstance*                      instance    = nullptr;

    VkPhysicalDeviceProperties        props       = {};
    VkPhysicalDeviceFeatures2         features    = {};
    // pNext chain for features2 (Vulkan11Features, Vulkan12Features, Vulkan13Features...)
    // We return embedded copies; only the game queries them.
    VkPhysicalDeviceVulkan11Features   vk11Features{};
    VkPhysicalDeviceVulkan12Features   vk12Features{};
    VkPhysicalDeviceVulkan13Features   vk13Features{};

    VkQueueFamilyProperties           queueFamilies[1] = {};
    uint32_t                          queueFamilyCount = 1;

    // Memory properties
    VkPhysicalDeviceMemoryProperties  memProps{};

    // Device extensions (reported)
    std::vector<VkExtensionProperties> deviceExtensions;
    // Instance extensions (reported)
    std::vector<VkExtensionProperties> instanceExtensions;
};

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------
struct NullDevice {
    uint32_t                          magic     = kMagicDevice;
    VkDevice                          handle    = nullptr;
    VkPhysicalDevice                   physDevice = nullptr;
    uint32_t                          apiVersion = VK_API_VERSION_1_3;

    // Queues
    static constexpr uint32_t kMaxQueues = 16;
    struct NullQueue*                  queues[kMaxQueues] = {};
    uint32_t                          queueCount = 0;
};

// ---------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------
struct NullQueue {
    uint32_t                          magic       = kMagicQueue;
    VkQueue                           handle      = nullptr;
    NullDevice*                       device      = nullptr;
    uint32_t                          familyIndex = 0;
    uint32_t                          queueIndex  = 0;
};

// ---------------------------------------------------------------------------
// Device memory and resources
// ---------------------------------------------------------------------------
struct NullDeviceMemory {
    uint32_t                          magic = kMagicMemory;
    VkDeviceMemory                    handle = VK_NULL_HANDLE;
    NullDevice*                       device = nullptr;
    VkDeviceSize                      allocationSize = 0;
    uint32_t                          memoryTypeIndex = 0;
    std::unique_ptr<uint8_t[]>        bytes;
    bool                              mapped = false;
};

struct NullBuffer {
    uint32_t                          magic = kMagicBuffer;
    VkBuffer                          handle = VK_NULL_HANDLE;
    NullDevice*                       device = nullptr;
    VkDeviceSize                      size = 0;
    VkBufferUsageFlags                usage = 0;
    VkBufferCreateFlags               flags = 0;
    NullDeviceMemory*                 memory = nullptr;
    VkDeviceSize                      memoryOffset = 0;
    VkDeviceAddress                   fakeDeviceAddress = 0;
};

struct NullImage {
    uint32_t                          magic = kMagicImage;
    VkImage                           handle = VK_NULL_HANDLE;
    NullDevice*                       device = nullptr;
    VkImageType                       imageType = VK_IMAGE_TYPE_2D;
    VkFormat                          format = VK_FORMAT_UNDEFINED;
    VkExtent3D                        extent{};
    uint32_t                          mipLevels = 1;
    uint32_t                          arrayLayers = 1;
    VkSampleCountFlagBits             samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageTiling                     tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags                 usage = 0;
    VkImageCreateFlags                flags = 0;
    VkImageLayout                     currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    NullDeviceMemory*                 memory = nullptr;
    VkDeviceSize                      memoryOffset = 0;
};

enum class CommandType {
    CopyBuffer,
    UpdateBuffer,
    FillBuffer,
    CopyBufferToImage,
    CopyImageToBuffer,
    CopyImage,
    PipelineBarrier,
};

struct RecordedCommand {
    CommandType                        type = CommandType::CopyBuffer;
    NullBuffer*                        srcBuffer = nullptr;
    NullBuffer*                        dstBuffer = nullptr;
    NullImage*                         srcImage = nullptr;
    NullImage*                         dstImage = nullptr;
    VkImageLayout                      srcImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout                      dstImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkDeviceSize                       offset = 0;
    VkDeviceSize                       size = 0;
    uint32_t                           fillValue = 0;
    std::vector<uint8_t>               data;
    std::vector<VkBufferCopy>          bufferCopies;
    std::vector<VkBufferImageCopy>     bufferImageCopies;
    std::vector<VkImageCopy>           imageCopies;
    std::vector<VkImageMemoryBarrier>  imageBarriers;
};

enum class CommandBufferState {
    Initial,
    Recording,
    Executable,
    Pending,
};

struct NullCommandPool {
    uint32_t                           magic = kMagicCommandPool;
    VkCommandPool                      handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    uint32_t                           queueFamilyIndex = 0;
    VkCommandPoolCreateFlags           flags = 0;
    std::vector<struct NullCommandBuffer*> commandBuffers;
};

struct NullCommandBuffer {
    uint32_t                           magic = kMagicCommandBuffer;
    VkCommandBuffer                    handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    NullCommandPool*                   pool = nullptr;
    VkCommandBufferLevel               level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBufferUsageFlags          usage = 0;
    CommandBufferState                 state = CommandBufferState::Initial;
    std::vector<RecordedCommand>       commands;
};

struct NullFence {
    uint32_t                           magic = kMagicFence;
    VkFence                            handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    std::atomic<bool>                  signaled{false};
};

struct NullSemaphore {
    uint32_t                           magic = kMagicSemaphore;
    VkSemaphore                        handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    std::atomic<bool>                  signaled{false};
};

struct NullSurface {
    uint32_t                           magic = kMagicSurface;
    VkSurfaceKHR                       handle = VK_NULL_HANDLE;
    NullInstance*                      instance = nullptr;
    void*                              window = nullptr;
};

struct NullSwapchain {
    uint32_t                           magic = kMagicSwapchain;
    VkSwapchainKHR                     handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    NullSurface*                       surface = nullptr;
    VkFormat                           format = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR                    colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D                         extent{};
    VkImageUsageFlags                  usage = 0;
    VkPresentModeKHR                   presentMode = VK_PRESENT_MODE_FIFO_KHR;
    std::vector<NullImage*>            images;
    std::vector<NullDeviceMemory*>     imageMemory;
    std::atomic<uint32_t>              nextImage{0};
};

struct NullSamplerYcbcrConversion {
    uint32_t                           magic = kMagicYcbcr;
    VkSamplerYcbcrConversion           handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    VkFormat                           format = VK_FORMAT_UNDEFINED;
};

struct NullSampler {
    uint32_t                           magic = kMagicSampler;
    VkSampler                          handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    VkSamplerCreateInfo                createInfo{};
};

struct NullImageView {
    uint32_t                           magic = kMagicImageView;
    VkImageView                        handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    NullImage*                         image = nullptr;
    VkImageViewType                    viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat                           format = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange            subresourceRange{};
};

struct NullBufferView {
    uint32_t                           magic = kMagicBufferView;
    VkBufferView                       handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    NullBuffer*                        buffer = nullptr;
    VkFormat                           format = VK_FORMAT_UNDEFINED;
    VkDeviceSize                       offset = 0;
    VkDeviceSize                       range = 0;
};

struct NullShaderModule {
    uint32_t                           magic = kMagicShaderModule;
    VkShaderModule                     handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    std::vector<uint32_t>              code;
};

struct NullDescriptorSetLayout {
    uint32_t                           magic = kMagicSetLayout;
    VkDescriptorSetLayout              handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    VkDescriptorSetLayoutCreateFlags   flags = 0;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct DescriptorSlot {
    VkDescriptorType                   type = VK_DESCRIPTOR_TYPE_SAMPLER;
    VkSampler                          sampler = VK_NULL_HANDLE;
    VkImageView                        imageView = VK_NULL_HANDLE;
    VkImageLayout                      imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkBuffer                           buffer = VK_NULL_HANDLE;
    VkDeviceSize                       offset = 0;
    VkDeviceSize                       range = 0;
    VkBufferView                       bufferView = VK_NULL_HANDLE;
};

struct DescriptorBindingState {
    uint32_t                           binding = 0;
    VkDescriptorType                   type = VK_DESCRIPTOR_TYPE_SAMPLER;
    std::vector<DescriptorSlot>        slots;
};

struct NullDescriptorPool;
struct NullDescriptorSet {
    uint32_t                           magic = kMagicDescriptorSet;
    VkDescriptorSet                    handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    NullDescriptorPool*                pool = nullptr;
    NullDescriptorSetLayout*           layout = nullptr;
    std::vector<DescriptorBindingState> bindings;
};

struct NullDescriptorPool {
    uint32_t                           magic = kMagicDescriptorPool;
    VkDescriptorPool                   handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    VkDescriptorPoolCreateFlags        flags = 0;
    uint32_t                           maxSets = 0;
    std::vector<VkDescriptorPoolSize>  capacities;
    std::vector<NullDescriptorSet*>    sets;
};

struct NullPipelineLayout {
    uint32_t                           magic = kMagicPipelineLayout;
    VkPipelineLayout                   handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    std::vector<NullDescriptorSetLayout*> setLayouts;
    std::vector<VkPushConstantRange>   pushConstantRanges;
};

struct NullPipelineCache {
    uint32_t                           magic = kMagicPipelineCache;
    VkPipelineCache                    handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    std::vector<uint8_t>               data;
};

struct NullPipeline {
    uint32_t                           magic = kMagicPipeline;
    VkPipeline                         handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    VkPipelineBindPoint                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    NullPipelineLayout*                layout = nullptr;
};

struct NullRenderPass {
    uint32_t                           magic = kMagicRenderPass;
    VkRenderPass                       handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    uint32_t                           attachmentCount = 0;
    uint32_t                           subpassCount = 0;
};

struct NullFramebuffer {
    uint32_t                           magic = kMagicFramebuffer;
    VkFramebuffer                      handle = VK_NULL_HANDLE;
    NullDevice*                        device = nullptr;
    NullRenderPass*                    renderPass = nullptr;
    uint32_t                           width = 0;
    uint32_t                           height = 0;
    uint32_t                           layers = 0;
    std::vector<NullImageView*>        attachments;
};

void SignalFence(NullFence* fence);
void SignalSemaphore(NullSemaphore* semaphore);
bool ConsumeSemaphore(NullSemaphore* semaphore);

// ---------------------------------------------------------------------------
// Handle validation helpers
// ---------------------------------------------------------------------------
template<typename T>
inline T* ValidateHandle(uint64_t handle, uint32_t expectedMagic) {
    auto* p = reinterpret_cast<T*>(handle);
    if (!p || p->magic != expectedMagic) return nullptr;
    return p;
}

template<typename T>
inline T* ValidateHandle(void* handle, uint32_t expectedMagic) {
    return ValidateHandle<T>(reinterpret_cast<uint64_t>(handle), expectedMagic);
}

// ---------------------------------------------------------------------------
// Initialize the physical device properties / features / memory with defaults.
// Call once during instance creation.
// ---------------------------------------------------------------------------
void InitPhysicalDevice(NullPhysicalDevice* pd);

} // namespace nv
