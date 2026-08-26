#pragma once
// nv_log.h -- minimal logging for the Null Vulkan backend.
// Writes to %TEMP%\nv_vulkan.log and OutputDebugStringA.

#include <cstdio>

namespace nv {

void LogInit();
void LogPrint(const char* fmt, ...);
void LogError(const char* fmt, ...);
// Log a stub call once (deduped per function name).
void LogStub(const char* fn);

} // namespace nv

#define NV_LOG(...) ::nv::LogPrint(__VA_ARGS__)
#define NV_STUB(fn) ::nv::LogStub(fn)
#define NV_ERR(...) ::nv::LogError(__VA_ARGS__)
