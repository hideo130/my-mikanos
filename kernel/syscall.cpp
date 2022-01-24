#include <array>
#include <cerrno>

#include "asmfunc.h"
#include "logger.hpp"
#include "msr.hpp"
#include "syscall.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "font.hpp"

namespace syscall
{
    struct Result
    {
        uint64_t value;
        int error;
    };

    namespace
    {
        template <class Func, class... Args>
        Result DoWinFunc(Func f, uint64_t flags_layer_id, Args... args)
        {
            const uint32_t layer_flags = flags_layer_id >> 32;
            const unsigned int layer_id = flags_layer_id & 0xffffffff;

            __asm__("cli");
            auto layer = layer_manager->FindLayer(layer_id);
            __asm__("sti");
            if (layer == nullptr)
            {
                return {0, EBADF};
            }

            const auto res = f(*layer->GetWindow(), args...);
            if (res.error)
            {
                return res;
            }

            // if bit 0 is 1 then we don't draw window.
            if ((layer_flags & 1) == 0)
            {
                __asm__("cli");
                layer_manager->Draw(layer_id);
                __asm__("sti");
            }
            return res;
        }
    }

#define SYSCALL(name)                                \
    Result name(                                     \
        uint64_t arg1, uint64_t arg2, uint64_t arg3, \
        uint64_t arg4, uint64_t arg5, uint64_t arg6)

    SYSCALL(LogString)
    {
        if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug)
        {
            return {0, EPERM};
        }
        const char *s = reinterpret_cast<const char *>(arg2);
        const auto len = strlen(s);

        if (len > 1024)
        {
            return {0, E2BIG};
        }
        Log(static_cast<LogLevel>(arg1), "%s", s);
        return {len, 0};
    }

    SYSCALL(PutString)
    {
        const auto fd = arg1;
        const char *s = reinterpret_cast<const char *>(arg2);
        const auto len = arg3;
        if (len > 1024)
        {
            return {0, E2BIG};
        }
        if (fd == 1)
        {
            const auto task_id = task_manager->CurrentTask().ID();
            (*terminals)[task_id]->Print(s, len);
            return {len, 0};
        }
        return {0, EBADF};
    }

    SYSCALL(Exit)
    {
        __asm__("cli");
        auto &task = task_manager->CurrentTask();
        __asm__("sti");
        // first arg is set to RAX, second arg is set to RDX
        return {task.OSStackPointer(), static_cast<int>(arg1)};
    }

    SYSCALL(OpenWindow)
    {
        const int w = arg1, h = arg2, x = arg3, y = arg4;
        const auto title = reinterpret_cast<const char *>(arg5);
        const auto win = std::make_shared<ToplevelWindow>(
            w, h, screen_config.pixel_format, title);
        __asm__("cli");
        const auto layer_id = layer_manager->NewLayer()
                                  .SetWindow(win)
                                  .SetDraggable(true)
                                  .Move({x, y})
                                  .ID();
        active_layer->Activate(layer_id);
        __asm__("sti");
        return {layer_id, 0};
    }

    SYSCALL(WinWriteString)
    {
        return DoWinFunc(
            [](Window &win,
               int x, int y, uint32_t color, const char *s)
            {
                WriteString(*win.Writer(), {x, y}, s, ToColor(color));
                return Result{0, 0};
            },
            arg1, arg2, arg3, arg4, reinterpret_cast<const char *>(arg5));
    }

    SYSCALL(WinFillRectangle)
    {
        return DoWinFunc(
            [](Window &win,
               int x, int y, int w, int h, uint32_t color)
            {
                FillRectangle(*win.Writer(), {x, y}, {w, h}, ToColor(color));
                return Result{0, 0};
            },
            arg1, arg2, arg3, arg4, arg5, arg6);
    }

    SYSCALL(WinRedraw)
    {
        return DoWinFunc(
            [](Window &)
            {
                return Result{0, 0};
            },
            arg1);
    }

    SYSCALL(GetCurrentTick)
    {
        return {timer_manager->CurrentTick(), kTimerFreq};
    }

#undef SYSCALL
}

using SyscallFuncType = syscall::Result(uint64_t, uint64_t, uint64_t,
                                        uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType *, 8> syscall_table{
    /* 0x00 */ syscall::LogString,
    /* 0x01 */ syscall::PutString,
    /* 0x02 */ syscall::Exit,
    /* 0x03 */ syscall::OpenWindow,
    /* 0x04 */ syscall::WinWriteString,
    /* 0x05 */ syscall::WinFillRectangle,
    /* 0x06 */ syscall::GetCurrentTick,
    /* 0x07 */ syscall::WinRedraw,
};

void InitializeSyscall()
{
    WriteMSR(kIA32_EFER, 0x0501u);
    WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
    WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                             static_cast<uint64_t>(16 | 3) << 48);
    WriteMSR(kIA32_FMASK, 0);
}
