cmake_minimum_required(VERSION 3.20)
project(nullvulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Sources ----
set(NV_SOURCES
    src/nv_log.cpp
    src/nv_objects.cpp
    src/vk_dispatch.cpp
    src/nv_instance.cpp
    src/nv_memory.cpp
    src/nv_commands.cpp
    src/nv_sync.cpp
    src/nv_swapchain.cpp
    src/nv_render_objects.cpp
    src/nv_formats.cpp
    src/generated/nv_generated.cpp
)

# ---- Generated .def (export table) ----
set(NV_DEF
    src/generated/vulkan-1.def
)

# ---- Shared library ----
add_library(vulkan-1 SHARED ${NV_SOURCES} ${NV_DEF})

target_include_directories(vulkan-1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Vulkan-Headers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan_headers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src/generated  # if generated lives there
)
target_compile_definitions(vulkan-1 PRIVATE VK_USE_PLATFORM_WIN32_KHR NOMINMAX)

# Disable auto-export — we use explicit .def
set_target_properties(vulkan-1 PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# MSVC specific: treat .def as module definition file
if(MSVC)
    set_source_files_properties(${NV_DEF} PROPERTIES
        HEADER_FILE_ONLY FALSE
        GENERATED TRUE
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(nullvulkan_tests tests/nullvulkan_tests.cpp)
    target_include_directories(nullvulkan_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Vulkan-Headers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan_headers/include
    )
    target_link_libraries(nullvulkan_tests PRIVATE vulkan-1)
    target_compile_definitions(nullvulkan_tests PRIVATE VK_USE_PLATFORM_WIN32_KHR NOMINMAX)
    add_test(NAME nullvulkan_api COMMAND nullvulkan_tests)
endif()
