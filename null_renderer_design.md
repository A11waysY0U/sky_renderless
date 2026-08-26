# Null Vulkan Backend 设计文档（Sky 光·遇 PC）

> 状态：逆向边界已闭合，进入实现设计阶段
> 目标：实现一个"资源状态正确、同步正确、数据流正确，但完全不执行图形计算的 Vulkan backend"，
> 用于在无 GPU 渲染的前提下让游戏完整运行（登录/场景/角色/网络/持续挂机）。

---

## 0. 一句话结论

**不是模拟 Vulkan GPU，而是维持一个 CPU 资源模型 + 正确的同步/数据流语义，把所有图形计算（Draw/Raster/Compute/Present）丢弃，把 GPU→CPU 读回替换为合法合成数据。**

技术形态：**一个放在游戏目录下的 Proxy `vulkan-1.dll`（CPU-only Null Backend）**，因为游戏对 vulkan-1.dll 是 `LoadLibraryA + GetProcAddress` 动态解析、**零静态导入**。

```text
Sky.exe
   │  LoadLibraryA("vulkan-1.dll")
   ▼
Proxy vulkan-1.dll (Null Backend)
   ├── Object creation ──────→ CPU object table
   ├── VMA memory ───────────→ CPU backing memory
   ├── Upload/Copy ──────────→ CPU memcpy / image conversion
   ├── Graphics commands ────→ discard
   ├── Compute commands ─────→ discard（待实验验证）
   ├── Readback ──────────────→ synthetic CPU data
   ├── Fence/Semaphore ──────→ CPU completion
   └── Present ───────────────→ discard
```

---

## 1. 目标与验收

### 1.1 目标

- 游戏可：启动 → 登录 → 进入场景 → 角色正常活动 → 网络正常 → 持续运行
- 画面变黑 / 云水异常 = **可接受**（挂机目标）
- 崩溃 / 卡死 / gameplay 状态异常 = **不可接受**（说明有 CPU 可见依赖未覆盖）

### 1.2 验收链（每个里程碑跑一遍）

```text
启动 → 登录 → 进入场景 → 角色移动 → 持续挂机
```

### 1.3 非目标

- 不做真实光栅化 / 纹理采样 / SPIR-V 执行
- 不实现 GPU 虚拟地址 / 真实 device address
- 不保证图像内容正确（只保证数据流合法、不 NaN/Inf、不越界）

---

## 2. 逆向结论摘要（证据基准）

| 主题 | 结论 | 证据 |
|---|---|---|
| 游戏身份 | 网易《光·遇》Sky (thatgamecompany)，PC `0.16.2.404410`，`com.netease.sky`，JIGUANG Series，自定义 Sky 引擎 | `Game Info`/`User-Agent` 字符串 |
| 图形 API | **Vulkan 1.3 实例**（min 1.2），**唯一渲染后端**（无 D3D11/12/GL） | `VulkanRenderer` 唯一实现；无 OpenGL/d3d12 字符串 |
| 加载方式 | `LoadLibraryA("vulkan-1.dll") + GetProcAddress`，**零静态导入** | 完整 IAT 无 vulkan-1.dll；`FUN_7ff766b1af40` |
| 渲染器类型 | 跨平台抽象：PC=win/`.spv`(Vulkan)、iOS=`.metallib`、PS4=`.sb`、Android=`.spv` | 着色器路径 `FUN_7ff764dbcd90` |
| 着色器 | **预编译 SPIR-V**，从 `Data/Shaders/<变体>/<名>.win.spv` 加载，Lua 定义（`ShaderDefs.lua`），反射驱动管线 | `"Vulkan requires precompiled shaders"` |
| VMA | **静态链接内嵌**，`VMA_DYNAMIC_VULKAN_FUNCTIONS`，allocator 带 `BUFFER_DEVICE_ADDRESS`(0x20)/`KHR_bind_memory2`(0x8)/`EXT_memory_budget`(0x10)，自定义 `UniformDynamic` 池 | `InitializeMemoryVMA`=`FUN_7ff766abb060` |
| BDA | 特性/分配器层**已启用**，运行期**无活跃消费者**（无 RT、无 `buffer_reference`，游戏表 `vkGetBufferDeviceAddress` 从未被读） | xref 分析 |
| Compute | **真实运行**：`vkCreateComputePipelines`(`FUN_7ff766ab9240`)+`vkCmdDispatch`(`FUN_7ff766aca8e0`)。pass：**Water Sim / Cloud / PreCompute** | 渲染图字符串 |
| RT | 入口点全解析但**从未调用**（KHR+NV 均无 READ） | xref |
| 网格着色器/动态渲染/Sync2/时间线信号量/二级命令缓冲 | **均只解析未调用** | xref |
| GPU 计时查询 | `vkCreateQueryPool`/`vkCmdWriteTimestamp` **未调用** | xref |
| 读回链 | 三条，最终消费者均不进入 gameplay（见 §6） | 逐层 xref |

### 2.1 实际调用的 API 子集（"调用"= 函数指针表有 READ xref）

**真实调用**：
- 实例/设备：`vkCreateInstance`、`vkEnumeratePhysicalDevices`、`vkGetPhysicalDeviceFeatures2`、`vkEnumerateDeviceExtensionProperties`、`vkCreateDevice`、`vkGetDeviceQueue`、`vkDestroyDevice`
- 命令录制：`vkBegin/EndCommandBuffer`、`vkCmdBegin/EndRenderPass`（传统）、`vkCmdBindPipeline/DescriptorSets/VertexBuffers/IndexBuffer`、`vkCmdDraw`/`DrawIndexed`/`DrawIndirect`/`DrawIndexedIndirect`、`vkCmdDispatch`、`vkCmdSetViewport/Scissor/DepthBias/Stencil*`、`vkCmdPipelineBarrier`（传统）、`vkCmdCopyBuffer/CopyImage/CopyBufferToImage/CopyImageToBuffer/BlitImage/FillBuffer/Clear*`、`vkCmdUpdateBuffer`
- 提交/同步：`vkQueueSubmit`（传统）、`vkQueueWaitIdle`、`vkDeviceWaitIdle`、`vkWaitForFences`（∞阻塞）
- 对象/描述符：`vkCreateGraphicsPipelines`、`vkCreateComputePipelines`、`vkCreatePipelineLayout/Cache`、`vkCreateRenderPass/Framebuffer`、`vkCreateDescriptorPool/SetLayout`、`vkAllocate/FreeDescriptorSets`、`vkUpdateDescriptorSets`、`vkCreateShaderModule`、`vkCreateSampler/ImageView/BufferView`、`vkCreateSwapchainKHR`、`vkAcquireNextImageKHR`、`vkGetSwapchainImagesKHR`、`vkQueuePresentKHR`

**只解析未调用（可返回 stub）**：RT 全家（KHR+NV）、`DispatchIndirect/Base`、`DrawIndirectCount/DrawIndexedIndirectCount`、`ExecuteCommands`、Mesh Shader、动态渲染、Sync2、时间线信号量、动态状态、`PushDescriptorSetKHR`/descriptor update template、GPU 计时查询、`GetBufferDeviceAddress`、`GetFenceStatus`、debug marker、`GetDeviceBufferMemoryRequirements`、private data、descriptor buffer、光流/CUDA NVX、HUAWEI/INTEL/QCOM/GOOGLE/AMD 厂商扩展。

---

## 3. 核心设计原则

> **Render path 可以 Null，resource semantics 不能 Null。**

| 层 | 处理 |
|---|---|
| Graphics Draw | 丢弃 |
| Graphics State | 只维护状态（绑定关系） |
| Compute | 首先尝试丢弃（PoC 假设） |
| CPU→GPU upload | CPU backing memory |
| GPU→CPU readback | 必须模拟正确结果（合成数据） |
| Fence/Semaphore | Null Queue 立即完成 + 正确回调 |
| Present | 丢弃 |

---

## 4. 注入机制（已确认）

### 4.1 关键事实

- 完整导入表**无 vulkan-1.dll 块**（只有 kernel32/user32/ws2_32/crypt32/winmm/advapi32/ole32/shell32/shcore/dxgi/xinput/gdi32/SetupAPI/dwmapi/fmod/MF/WinHTTP）
- 启动加载器 `FUN_7ff766b1af40`：`LoadLibraryA("vulkan-1.dll")` → 解析 `vkGetInstanceProcAddr`/`vkCreateInstance`/`vkEnumerateInstance*`，失败返回 `0xfffffffd`（`VK_ERROR_INCOMPATIBLE_DRIVER` 风格）
- 全量函数经 `vkGetInstanceProcAddr` + `vkGetDeviceProcAddr` 双 loader（`FUN_7ff766b1b030`/`FUN_7ff766b1e460`）解析进同一张全局表

### 4.2 注入方案：Proxy vulkan-1.dll

```text
Sky.exe 目录 / DLL 搜索路径
   └── vulkan-1.dll（我们的 Null Backend）
         ├── 导出 vkGetInstanceProcAddr / vkGetDeviceProcAddr
         ├── （建议顺带导出 vk_icd* 符号，备用 ICD 路径）
         └── vkCreateInstance 返回我们自己的 instance 句柄
```

- 不需要 detours / IAT patch / 进程注入
- VMA 的 `VMA_DYNAMIC_VULKAN_FUNCTIONS` 会经**我们的** `vkGetInstanceProcAddr`/`vkGetDeviceProcAddr` 解析内存函数 → **我们的 vkAllocateMemory/vkMapMemory/vkCreateBuffer 就是 CPU backing 本体**
- 风险：NetEase 反作弊可能校验模块签名（PoC/离线可接受）
- 备选：真实 loader + `VK_ICD_FILENAMES` 指向 Null ICD（更正统，但 PoC 不必要）

---

## 5. Null Backend 架构

### 5.1 总览

```text
┌──────────────────────────────────────────┐
│               Sky.exe                    │
└───────────────────┬──────────────────────┘
                    │ Vulkan (LoadLibraryA)
                    ▼
┌──────────────────────────────────────────┐
│         Null Vulkan Backend (proxy)      │
│                                          │
│  Instance / Device / Queue               │
│  Buffer / Image / Memory                 │
│  Descriptor / Pipeline / RenderPass      │
│  CommandBuffer                           │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │ CPU Resource Store                 │  │
│  │  Buffer → byte storage             │  │
│  │  Image  → metadata / lazy storage  │  │
│  └────────────────────────────────────┘  │
│                                          │
│  Draw / Rasterization       → DROP       │
│  Compute                    → DROP*      │
│  Present                    → DROP       │
│  Copy                       → CPU        │
│  Readback                   → Synthetic  │
│  Fence/Semaphore            → CPU        │
└──────────────────────────────────────────┘
```

### 5.2 三层资源模型

```text
             Vulkan Handle
                   │
                   ▼
             Null Object
                   │
         ┌─────────┴─────────┐
         │                   │
     GPU-visible          CPU backing
       state                 data
         │                   │
         ▼                   ▼
    layout/usage          byte array
    descriptors           mapped ptr
    bindings              readback
```

例如 `NullImage`：

```cpp
struct NullImage {
    VkFormat format;
    uint32_t width, height, mipLevels, arrayLayers;
    VkImageUsageFlags usage;
    VkImageLayout currentLayout;      // 布局跟踪（§5.4）
    // lazy backing（§5.5）
    std::optional<std::vector<uint8_t>> backingStorage;
};
```

### 5.3 Null Queue = CPU execution engine

```text
vkQueueSubmit()
    │
    ▼
NullQueue::Submit
    ├── 遍历 command buffer 状态
    ├── 解析 semaphore 依赖（立即满足）
    ├── 应用 resource barriers（布局/状态）
    ├── 执行 CPU copies（memcpy）
    ├── 生成 synthetic readback
    └── signal fence（CPU 完成）
```

- `vkQueueWaitIdle` / `vkDeviceWaitIdle` / `vkWaitForFences` 表示"所有 CPU-visible work 已完成"，**不要简单返回 VK_SUCCESS**，要真正跑完 pending work + 回调队列。
- 关键函数 `FUN_7ff766abf4d0`：等 fence(∞) → 执行挂起读回回调 → 释放帧内存。这个回调队列语义必须保留。

### 5.4 布局/状态跟踪（PipelineBarrier）

不做真实 cache/barrier，只维护逻辑状态：

```text
UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY → TRANSFER_SRC ...
```

- `NullImage.currentLayout` 由 `vkCmdPipelineBarrier`（oldLayout/newLayout）与 `vkCmdCopyImage`/`vkCmdCopyImageToBuffer` 等维护
- 供 `CopyImageToBuffer` 判断源状态（可简化：只要内存合法即可）

### 5.5 Lazy backing store（性能优化，可选）

```text
vkCreateImage          → metadata only（不分配像素）
vkCmdDraw              → nothing
vkCmdCopyImageToBuffer → 首次发现需要读回 → 生成 synthetic data
```

降低 i5-5250U 内存带宽压力。**注意**：若 buffer 是 VMA 持久映射（`VMA_ALLOCATION_CREATE_MAPPED_BIT`），backing 必须在创建时就存在（`CreateBuffer` 断言 `"Buffer %s wasn't created mapped?"`）。

---

## 6. 读回策略（已确认，不进入 gameplay）

### 6.1 三条读回链及最终消费者

| 读回链 | 分类 | 最终消费者 | 证据链 |
|---|---|---|---|
| **Screenshot** | A（纯录制） | 图片文件（VFS 写入） | `rtScreenshot` → `FUN_7ff766394150` → `FUN_7ff764da21a0`(SaveTexture job) → `FUN_7ff764d59af0`(job 回调) → `CopyImageToBuffer` → fence → map → `FUN_7ff766ae0730`(写文件) |
| **Video** | A（录制+曝光反馈） | 视频编码器 + 录像曝光调参 | 主帧走 D3D11 共享句柄（`CCGameRecord64.dll`）；`rtVideoReadback`(2×2)+`VideoDownscaleFeedback` 调整"exposure and tonemapping to improve video colors" |
| **Luminance** | 渲染器闭环 | **自动曝光/眼睛适应**（`u_averageLum`/`u_averageLumChangeTm`） | `rtLuminanceReadback`(2×2) → `LumFeedback/FragSSBO` → CPU 均亮 → 后处理帧图 `FUN_7ff76637f930` |
| **Depth** | 渲染器闭环 | 深度后处理微调（`DepthFeedback/FragSSBO` → `tag_DepthReadback` 资源） | `rtDepthReadback`(2×2) → `DepthFeedback` → 资源 Update 分发（无 gameplay 消费者） |

**结论：没有任何一条链流入 gameplay / AI / physics / network。B/C 类风险不存在。**

### 6.2 合成数据建议

| 读回 | 建议值 | 理由 |
|---|---|---|
| `rtScreenshot` / `rtVideoReadback` | `RGBA = 0` | 纯录制 |
| `rtLuminanceReadback` | **平均亮度 ≈ 0.5** | 自动曝光闭环稳定收敛，避免 HDR/tonemap 参数发散 |
| `rtDepthReadback` | 固定合法深度值 | 见下 |

**深度值注意**：不要假定"远平面=1.0"。必须按 readback image 的 format / component type / 期望范围 / shader decode 方式生成合法 backing，避免 `NaN / Inf / 越界`。

### 6.3 FragSSBO 必须当成真 Buffer

`LumFeedbackFragSSBO` / `DepthFeedbackFragSSBO` / `VideoDownscaleFeedbackFragSSBO`（特性开启时反馈写 storage buffer）——**不要做成假空对象**，而是：

```text
VkBuffer → NullBuffer → CPU byte array（预置固定值）
    ↓
VMA allocation / persistent mapping / Map / Flush / Invalidate / Unmap / Free 全部保持正常
```

两条路径（Readback 纹理 与 FragSSBO）都要覆盖。

### 6.4 Map/Invalidate/Flush 语义（关键实现细节）

- `vkMapMemory` / `vkInvalidateMappedMemoryRanges` / `vkFlushMappedMemoryRanges` **游戏代码不直接调用**，全部经 VMA 内部指针（`VMA_DYNAMIC_VULKAN_FUNCTIONS`）。
- 游戏侧触发映射的入口：
  - a. 持久映射：`CreateBuffer` 用 `VMA_ALLOCATION_CREATE_MAPPED_BIT`（staging / 动态 uniform）
  - b. CopyBuffer CPU 快路径：`FUN_7ff766ac3200`（虚函数 Map@vtable+0x250 → memcpy → Unmap/Flush@vtable+0x258）
  - c. 读回：fence 完成后 map 读 CPU 数据
- 因此 backend 只需保证 VMA 经我们的 `vkGetDeviceProcAddr` 能解析到这些函数，并让它们落到 CPU backing 即可。

---

## 7. 后端契约（实现必须满足的硬约束）

从 `VulkanInit`/`InitializeMemoryVMA`/`CreateDevice` 的断言与流程直接提炼：

| 项 | 必须满足 |
|---|---|
| 版本 | `vkEnumerateInstanceVersion` ≥ 1.2（0x402000）；实例按 1.3（0x403000）创建 |
| 设备 | 枚举 ≥1 个 GPU；**报成 Discrete（DeviceType 2）** 以保证被选中（`"Found %s gpu with rating %d"`） |
| 特性 | `vkGetPhysicalDeviceFeatures2` 正确填 `VkPhysicalDeviceVulkan11/12/13Features`（sType `0x32/0x34/0x36`）；**`bufferDeviceAddress=VK_TRUE`**（否则 VMA 不置 BDA flag，`CreateBuffer` 校验失败） |
| 扩展 | 至少报：`VK_KHR_surface`、`VK_KHR_win32_surface`、`VK_KHR_swapchain`、`VK_KHR_shared_presentable_image`、`VK_KHR_external_memory(_win32)`、`VK_KHR_dedicated_allocation`、`VK_KHR_bind_memory2`、`VK_KHR_buffer_device_address`、`VK_EXT_memory_budget`、`VK_KHR_16bit_storage`、`VK_KHR_shader_float16_int8`、`VK_KHR_format_feature_flags2`、`VK_KHR_maintenance4`、`VK_KHR_sampler_ycbcr_conversion`、`VK_KHR_get_surface_capabilities2`、`VK_EXT_swapchain_colorspace`、`VK_EXT_debug_utils`、`VK_EXT_sampler_filter_minmax` |
| VMA 解析 | `vkGetDeviceProcAddr` 对 VMA 断言的函数（`vkGetBufferMemoryRequirements`/`vkGetImageMemoryRequirements`/`vkMapMemory`/`vkFlushMappedMemoryRanges`/`vkInvalidateMappedMemoryRanges`/`vkAllocateMemory`/`vkFreeMemory`/`vkCreateBuffer`/`vkCreateImage`/`vkBindBufferMemory`/`vkCmdCopyBuffer`/`vkGetPhysicalDeviceProperties`…）必须返回非空 |
| 交换链 | `vkCreateWin32SurfaceKHR`+`vkCreateSwapchainKHR`（真 HWND）→ `vkGetSwapchainImagesKHR` 返回**真实 VkImage 句柄** → `vkAcquireNextImageKHR` 返回 `VK_SUCCESS` 并循环 imageIndex → `vkQueuePresentKHR` 返回 `VK_SUCCESS` |
| 入口 | `vkGetInstanceProcAddr` 与 `vkGetDeviceProcAddr` **都要**正确返回（双 loader 写同一张表） |
| 外部内存 | `ImportMemoryWin32Handle` 有优雅失败分支（`"ImportMemoryWin32Handle is unsupported"` → 0xffffffff），第一版录像互操作可走失败路径豁免 |

---

## 8. API 实现范围分组

### 8.1 必须有真实语义

```text
Instance / PhysicalDevice / Device / Queue
Memory / Buffer / Image
Descriptor / PipelineLayout / Pipeline / ShaderModule
CommandPool / CommandBuffer
Fence / Semaphore
Swapchain
```

### 8.2 必须接受，但可"逻辑执行"

```text
Begin/EndCommandBuffer
Begin/EndRenderPass
PipelineBarrier（维护布局）
BindPipeline / BindDescriptorSets / BindVertexBuffers / BindIndexBuffer
SetViewport / SetScissor / SetDepthBias / SetStencil*
UpdateDescriptorSets
```

### 8.3 可直接 NO-OP

```text
Draw / DrawIndexed / DrawIndirect / DrawIndexedIndirect
Dispatch（PoC 假设）
Clear* / Blit（注意：可能参与 CPU-visible flow，见 §6）
```

> 注：`Clear/Blit/Copy` **不要一概 NO-OP**——它们可能参与 CPU-visible resource flow（mip 生成、读回前置 copy）。

### 8.4 Copy 家族的语义映射

| API | 调用点 | Null 语义 |
|---|---|---|
| `vkCmdCopyBuffer` | `FUN_7ff766ac3200` | CPU `memcpy`（含持久映射快路径） |
| `vkCmdCopyImage` | `FUN_7ff766accaa0` | CPU 图像搬运（或仅布局转换） |
| `vkCmdCopyBufferToImage` | `FUN_7ff766ac70a0`（上传，mip/数组） | 写入 image backing |
| `vkCmdCopyImageToBuffer` | `FUN_7ff766acc7e0`（读回） | 从 image backing 读出 / 生成合成数据 |
| `vkCmdBlitImage` | `FUN_7ff766abf8f0`/`ac0110`/`ac0f10`/`ac7a60`（mip 生成） | 可 NO-OP（或 CPU 简单搬运） |

---

## 9. BDA 最小兼容

- 已证明：游戏自身 `vkGetBufferDeviceAddress` 无热路径调用；无 RT；无 `buffer_reference`；BDA 主要是 VMA allocator flag。
- 第一版只需：`VkPhysicalDeviceVulkan12Features.bufferDeviceAddress = VK_TRUE`（与 VMA 初始化预期一致）。
- 若 VMA 后续真的请求某 buffer 的 device address，提供稳定虚拟地址映射：
  ```text
  NullBuffer → uint64_t fakeDeviceAddress
  ```
- **不需要实现真实 GPU 虚拟地址。**

---

## 10. 风险登记表

| # | 风险 | 等级 | 缓解 |
|---|---|---|---|
| 1 | **Compute NO-OP 后存在隐藏 CPU 逻辑依赖**（Water/Cloud/PreCompute 输出被 CPU 读取） | **高（唯一未知项）** | 第一版直接 Compute NO-OP，跑 §1.2 验收链；仅当崩溃/卡死/gameplay 异常才回查 |
| 2 | Luminance 反馈返回值导致 HDR/tonemap 参数发散 | 低 | 返回 0.5 中灰，闭环稳定 |
| 3 | Depth 合成值 NaN/Inf/越界 | 低 | 按 format/范围生成合法值（§6.2） |
| 4 | 交换链/表面创建失败导致引擎初始化失败 | 低 | 用真 HWND + 最小 swapchain（2 张 image） |
| 5 | NetEase 反作弊校验 DLL 签名 | 中 | PoC/离线场景可接受；备选 ICD 路径 |
| 6 | VMA 断言某个函数指针为空 | 低 | §7 VMA 解析清单全覆盖 |
| 7 | 录像（CCGameRecord64.dll）启动触发外部内存路径 | 中 | 走 `ImportMemoryWin32Handle is unsupported` 优雅失败 |

**已排除项**（无需实现）：RT、Mesh Shader、动态渲染、Sync2、时间线信号量、`ExecuteCommands`、`DispatchIndirect`、`DrawIndirectCount`、GPU 计时查询、游戏侧直接 memory mapping、活跃 BDA consumer。

---

## 11. 里程碑

| 阶段 | 内容 | 出口条件 |
|---|---|---|
| **M0** | 设计文档（本文档） | 定稿 |
| **M1** | Proxy vulkan-1.dll 骨架：导出层 + `vkGetInstanceProcAddr`/`vkGetDeviceProcAddr` + Instance/Device/Queue + 对象表 | DLL 可加载，游戏进入 VulkanInit |
| **M2** | 内存/资源：VMA 兼容内存函数 + Buffer/Image backing + 布局跟踪 + Copy 家族 CPU 语义 | 游戏初始化渲染器不崩 |
| **M3** | 同步/交换链：Fence/Semaphore 立即完成 + 回调队列 + Swapchain/Present | 进入主循环，Present 正常 |
| **M4** | Compute NO-OP 实验（§1.2 验收链） | 启动→登录→场景→挂机通过 |
| **M5** | 读回合成数据（§6.2） | 截图/视频/深度/亮度读回返回合法数据 |
| **M6** | Lazy backing + 性能优化 | 低内存带宽下稳定挂机 |

---

## 12. 下一步

1. （可选）Compute 依赖审计：把 Water Sim / Cloud / PreCompute 三个 pass 的输出是否有 CPU 回读 / gameplay 分支追完，钉死最后未知项。
2. 搭建 M1 骨架（Proxy vulkan-1.dll）。
3. 依 M2→M6 推进，每个里程碑跑验收链。
