# Sky Renderless

Sky Renderless 是一个面向 Windows x64 的 CPU-only Null Vulkan 后端。它生成一个可放置在目标程序旁的 `vulkan-1.dll`，维护程序运行所需的 Vulkan 对象、资源、数据传输与同步语义，但不执行真实的图形渲染或计算着色。

> [!WARNING]
> 本项目依据 Sky PC 客户端的实际 Vulkan 调用路径开发，主要用于研究、兼容性验证和无画面运行。它不是完整的 Vulkan 驱动，也不保证兼容其他 Vulkan 应用。

## 当前能力

- Vulkan 1.3 实例、物理设备、逻辑设备和队列
- CPU 后备的设备内存、缓冲区、图像及内存映射
- VMA 所需的内存与资源接口
- 命令池、命令缓冲区及 CPU 端复制、填充和清除操作
- 图像布局与基础资源状态跟踪
- Fence、Semaphore、Queue Submit 与立即完成模型
- Win32 Surface、Swapchain、图像获取和 Null Present
- Descriptor、Shader Module、Pipeline、Render Pass 和 Framebuffer 对象
- 物理设备格式与图像格式能力查询
- 针对不完整 R16 staging upload 的边界保护与零值尾部补全

Draw、Rasterization、Compute 和 Present 不产生真实画面或 GPU 工作。相关命令只保留应用继续运行所需的状态与生命周期语义。

## 架构概览

```text
Application
    | LoadLibrary("vulkan-1.dll")
    v
Null Vulkan dispatch
    |-- object and resource model
    |-- CPU-backed memory and copies
    |-- command recording and immediate queue execution
    |-- CPU synchronization
    `-- discarded draw / compute / present
```

实现代码位于 `src/`，API 测试位于 `tests/`，Vulkan 导出表及分发表生成器位于 `tools/`。更完整的背景、调用范围和设计依据见 [`null_renderer_design.md`](null_renderer_design.md)。

## 环境要求

- Windows x64
- Visual Studio 2022 或带 MSVC 工具链的 Build Tools
- CMake 3.20 或更高版本
- PowerShell 5.1 或更高版本

仓库已包含构建所需的 Vulkan Headers。

## Clone

`Vulkan-Headers` is pinned as a Git submodule. Clone the repository recursively:

```powershell
git clone --recurse-submodules https://github.com/A11waysY0U/sky_renderless.git
cd sky_renderless
```

For an existing clone, initialize the dependency before building:

```powershell
git submodule update --init --recursive
```

## 构建

在 Visual Studio Developer PowerShell 或已配置 MSVC 环境的终端中运行：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

输出文件：

```text
build\Release\vulkan-1.dll
```

## 测试

```powershell
ctest --test-dir build -C Release --output-on-failure
```

也可以直接运行测试程序：

```powershell
.\build\Release\nullvulkan_tests.exe
```

## 使用

将 `build\Release\vulkan-1.dll` 放到目标可执行文件所在目录，使程序通过 `LoadLibrary("vulkan-1.dll")` 加载本后端。建议先备份该目录中原有的同名 DLL。

运行日志写入：

```text
%TEMP%\nv_vulkan.log
```

## 重新生成 Vulkan 分发代码

修改生成规则或 Vulkan Headers 后运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\gen_vulkan.ps1
```

生成结果位于 `src/generated/`，并作为源码提交到仓库。

## 已知限制

- 不执行光栅化、纹理采样、SPIR-V 或计算着色器
- 不显示真实 Swapchain 内容，Present 仅维持调用流程
- Vulkan API 覆盖不完整；未建模的入口可能以兼容性 stub 返回
- GPU 到 CPU 的读回内容可能为合成数据，不保证图像或计算结果正确
- 某些截断上传会复制现有字节并用零补全缺失尾部，以保证范围安全和队列继续运行
- 资源与同步模型针对当前已观察到的 Sky 调用路径，不构成 Vulkan 规范一致性保证

