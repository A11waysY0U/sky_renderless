// nv_log.cpp
#include "nv_log.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <mutex>

static FILE* g_log = nullptr;
static std::mutex g_mtx;

// Track per-function stub logging (static set of pointers; simple table).
// We use a fixed-size array of "logged once" markers keyed by fn name.
// This is a quick-and-dirty dedup; log entries are ~16 bytes each.
static constexpr int kMaxLogged = 256;
static const char* g_logged[kMaxLogged] = {};
static int g_loggedCount = 0;

void nv::LogInit() {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_log) return;
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strcat_s(path, "nv_vulkan.log");
    fopen_s(&g_log, path, "w");
    if (g_log) {
        fprintf(g_log, "=== Null Vulkan Backend ===\n");
        fflush(g_log);
    }
    OutputDebugStringA("[nv] Null Vulkan Backend initializing\n");
}

void nv::LogPrint(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

void nv::LogError(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(g_log, "[ERROR] ");
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

void nv::LogStub(const char* fn) {
    // Log first occurrence only.
    bool first = false;
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        for (int i = 0; i < g_loggedCount; ++i) {
            if (strcmp(g_logged[i], fn) == 0) { first = false; break; }
        }
        // Not found — first time.
        if (g_loggedCount < kMaxLogged) {
            g_logged[g_loggedCount++] = fn;
            first = true;
        }
    }
    if (first) {
        NV_LOG("[STUB] %s", fn);
        char buf[512];
        snprintf(buf, sizeof(buf), "[nv:STUB] %s\n", fn);
        OutputDebugStringA(buf);
    }
}