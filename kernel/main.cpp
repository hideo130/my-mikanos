#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <array>
#include <deque>
#include <numeric>
#include <vector>

#include "asmfunc.h"
#include "acpi.hpp"
#include "fat.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "keyboard.hpp"
#include "layer.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "mouse.hpp"
#include "queue.hpp"
#include "segment.hpp"
#include "syscall.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "timer.hpp"
#include "font.hpp"
#include "console.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "window.hpp"
#include "logger.hpp"
#include "usb/xhci/xhci.hpp"

// void *operator new(std::size_t size, void *buf)
// {
//     return buf;
// }

void operator delete(void *obj) noexcept {}

int printk(const char *format, ...)
{
    va_list ap;
    int result;
    char s[1024];
    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}

extern "C" void __cxa_pure_virtual()
{
    while (1)
        __asm__("hlt");
}

std::shared_ptr<ToplevelWindow> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow()
{
    main_window = std::make_shared<ToplevelWindow>(
        160, 52, screen_config.pixel_format, "Hello Window");

    main_window_layer_id = layer_manager->NewLayer()
                               .SetWindow(main_window)
                               .SetDraggable(true)
                               .Move({300, 100})
                               .ID();
    layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<ToplevelWindow> text_window;
unsigned int text_window_layer_id;
void InitializeTextWindow()
{
    const int win_w = 160;
    const int win_h = 52;

    text_window = std::make_shared<ToplevelWindow>(
        win_w, win_h, screen_config.pixel_format, "Text Box Test");
    DrawTextbox(*text_window->InnerWriter(), {0, 0}, text_window->InnerSize());

    text_window_layer_id = layer_manager->NewLayer()
                               .SetWindow(text_window)
                               .SetDraggable(true)
                               .Move({500, 100})
                               .ID();
    layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

int text_window_index;

void DrawTextCursor(bool visible)
{
    const auto color = visible ? ToColor(0) : ToColor(0xffffff);
    const auto pos = Vector2D<int>{4 + 8 * text_window_index, 5};
    FillRectangle(*text_window->InnerWriter(), pos, {7, 15}, color);
}

void InputTextWindow(char c)
{
    if (c == 0)
    {
        return;
    }

    // why add 6?
    auto pos = []()
    { return Vector2D<int>{4 + 8 * text_window_index, 6}; };

    const int max_chars = (text_window->InnerSize().x - 8) / 8 - 1;
    if (c == '\b' && text_window_index > 0)
    {
        DrawTextCursor(false);
        text_window_index--;
        // fill white color for 1 charactor
        FillRectangle(*text_window->InnerWriter(), pos(), {8, 16}, ToColor(0xffffff));
        DrawTextCursor(true);
    }
    else if (c >= ' ' && text_window_index < max_chars)
    {
        DrawTextCursor(false);
        WriteAscii(*text_window->InnerWriter(), pos(), c, ToColor(0));
        text_window_index++;
        DrawTextCursor(true);
    }
    layer_manager->Draw(text_window_layer_id);
}

// std::deque<Message> *main_queue;

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void
KernelMainNewStack(const FrameBufferConfig &frame_buffer_config_ref,
                   const MemoryMap &memory_map_ref,
                   const acpi::RSDP &acpi_table,
                   void *volume_image)
{

    MemoryMap memory_map{memory_map_ref};

    InitializeGraphics(frame_buffer_config_ref);
    InitializeConsole();
    // WriteAscii(*pixel_writer, 58, 50, 'a', {0, 0, 0});
    // WriteString(*pixel_writer, 100, 66, "Hello World!", {0, 0, 255});
    // char buf[128];
    // sprintf(buf, "1+2=%d", 1 + 2);
    // WriteString(*pixel_writer, 100, 300, buf, {0, 0, 255});

    printk("Welcom to MyMikcanos!\n");
    SetLogLevel(kWarn);

    InitializeSegmentation();
    SetupIdentityPageTable();

    InitializeMemoryManager(memory_map);
    printk("memory_map: %p\n", &memory_map);
    InitializeTSS();

    InitializeInterrupt();
    fat::Initialize(volume_image);
    InitializePCI();

    InitializeLayer();
    InitializeMainWindow();
    InitializeTextWindow();
    layer_manager->Draw({{0, 0}, ScreenSize()});

    acpi::Initialize(acpi_table);
    InitializeLAPICTimer();

    char str[128];

    const int kTextboxCursorTimer = 1;
    const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
    timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTimer, 1});
    bool textbox_cursor_visible = false;

    InitializeSyscall();

    InitializeTask();

    usb::xhci::Initialize();
    InitializeKeyboard();
    InitializeMouse();
    Task &main_task = task_manager->CurrentTask();
    task_manager->NewTask()
        .InitContext(TaskTerminal, 0)
        .Wakeup();

    // uint8_t *p = reinterpret_cast<uint8_t *>(volume_image);
    // printk("Volume Image:\n");
    // for (int i = 0; i < 16; i++)
    // {
    //     printk("%04x:", i * 16);
    //     for (int j = 0; j < 8; j++)
    //     {
    //         printk(" %02x", *p);
    //         p++;
    //     }
    //     printk(" ");
    //     for (int j = 0; j < 8; j++)
    //     {
    //         printk(" %02x", *p);
    //         p++;
    //     }
    //     printk("\n");
    // }

    while (1)
    {
        __asm__("cli");
        const auto tick = timer_manager->CurrentTick();
        __asm__("sti");

        sprintf(str, "%010lu", tick);
        FillRectangle(*main_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*main_window->InnerWriter(), {20, 4}, str, {0, 0, 0});
        layer_manager->Draw(main_window_layer_id);

        __asm__("cli");
        auto msg = main_task.ReceiveMessage();
        if (!msg)
        {
            main_task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (msg->type)
        {
        case Message::kInterruptXHCI:
            usb::xhci::ProcessEvents();
            break;
        case Message::kTimerTimeout:
            if (msg->arg.timer.value == kTextboxCursorTimer)
            {
                __asm__("cli");
                timer_manager->AddTimer(Timer(msg->arg.timer.timeout + kTimer05Sec, kTextboxCursorTimer, 1));
                __asm__("sti");
                textbox_cursor_visible = !textbox_cursor_visible;
                DrawTextCursor(textbox_cursor_visible);
                layer_manager->Draw(text_window_layer_id);
            }
            break;
        case Message::kKeyPush:
            if (auto act = active_layer->GetActive(); act == text_window_layer_id)
            {
                if (msg->arg.keyboard.press)
                {
                    InputTextWindow(msg->arg.keyboard.ascii);
                }
            }
            else if (msg->arg.keyboard.press && msg->arg.keyboard.keycode == 59 /* F2*/)
            {
                task_manager->NewTask()
                    .InitContext(TaskTerminal, 0)
                    .Wakeup();
            }
            else
            {
                __asm__("cli");
                auto task_it = layer_task_map->find(act);
                __asm__("sti");
                if (task_it != layer_task_map->end())
                {
                    __asm__("cli");
                    task_manager->SendMessage(task_it->second, *msg);
                    __asm__("sti");
                }
                else
                {
                    printk("key push not handle : keycode %02x, ascii %02x\n",
                           msg->arg.keyboard.keycode,
                           msg->arg.keyboard.ascii);
                }
            }
            break;
        case Message::kLayer:
            ProcessLayerMessage(*msg);
            __asm__("cli");
            task_manager->SendMessage(msg->src_task, Message{Message::kLayerFinish});
            __asm__("sti");
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg->type);
        }
    }
}
