#ifdef __cplusplus
#include <cstdint>
#include <cstddef>

extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "../kernel/logger.hpp"
#include "../kernel/app_event.hpp"

    struct SyscallResult
    {
        uint64_t value;
        int error;
    };

    struct SyscallResult SyscallLogString(enum LogLevel level, const char *message);
    struct SyscallResult SyscallPutString(int fd, const char *s, size_t len);
    void SyscallExit(int exit_code);
    struct SyscallResult SyscallOpenWindow(int w, int h, int x, int y, const char *title);
#define LAYER_NO_REDRAW (0x00000001ull << 32)
    struct SyscallResult SyscallWinWriteString(uint64_t flags_layer_id, int x, int y, uint32_t color, const char *text);
    struct SyscallResult SyscallWinFillRectangle(uint64_t flags_layer_id, int x, int y, int w, int h, uint32_t color);
    struct SyscallResult SyscallGetCurrentTick();
    struct SyscallResult SyscallWinRedraw(uint64_t flags_layer_id);
    struct SyscallResult SyscallWinDrawLine(uint64_t flags_layer_id, int x0, int y0, int x1, int y1, uint32_t color);
    struct SyscallResult SyscallCloseWindow(uint64_t layer_id);
    struct SyscallResult SyscallReadEvent(struct AppEvent *events, size_t len);
    struct SyscallResult SyscallOpenFile(const char *path, int flags);
    struct SyscallResult SyscallReadFile(int fd, void *buf, size_t count);

#define TIMER_ONESHOT_REL 1
#define TIMER_ONESHOT_ABS 0
    struct SyscallResult SyscallCreateTimer(
        unsigned int type, int timer_value, unsigned long timeout_ms);

#ifdef __cplusplus
} // extern "C"
#endif
