#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <array>
#include <deque>
#include <numeric>
#include <vector>

#include "asmfunc.h"
#include "acpi.hpp"
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
#include "task.hpp"
#include "timer.hpp"
#include "font.hpp"
#include "console.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "window.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

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

unsigned int mouse_layer_id;
Vector2D<int> screen_size;
Vector2D<int> mouse_position;

extern "C" void __cxa_pure_virtual()
{
    while (1)
        __asm__("hlt");
}

std::shared_ptr<Window> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow()
{
    main_window = std::make_shared<Window>(
        160, 52, screen_config.pixel_format);
    DrawWindow(*main_window->Writer(), "Hello Window");
    main_window_layer_id = layer_manager->NewLayer()
                               .SetWindow(main_window)
                               .SetDraggable(true)
                               .Move({300, 100})
                               .ID();
    layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<Window> text_window;
unsigned int text_window_layer_id;
void InitializeTextWindow()
{
    const int win_w = 160;
    const int win_h = 52;

    text_window = std::make_shared<Window>(
        win_w, win_h, screen_config.pixel_format);
    DrawWindow(*text_window->Writer(), "Text Box Test");
    DrawTextBox(*text_window->Writer(), {4, 24}, {win_w - 8, win_h - 24 - 4});

    text_window_layer_id = layer_manager->NewLayer()
                               .SetWindow(text_window)
                               .SetDraggable(true)
                               .Move({350, 200})
                               .ID();
    layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

int text_window_index;

void DrawTextCursor(bool visible)
{
    const auto color = visible ? ToColor(0) : ToColor(0xffffff);
    const auto pos = Vector2D<int>{8 + 8 * text_window_index, 24 + 5};
    FillRectangle(*text_window->Writer(), pos, {7, 15}, color);
}

void InputTextWindow(char c)
{
    if (c == 0)
    {
        return;
    }

    // why add 6?
    auto pos = []()
    { return Vector2D<int>{8 + 8 * text_window_index, 24 + 6}; };

    const int max_chars = (text_window->Width() - 16) / 8;
    if (c == '\b' && text_window_index > 0)
    {
        DrawTextCursor(false);
        text_window_index--;
        // fill white color for 1 charactor
        FillRectangle(*text_window->Writer(), pos(), {8, 16}, ToColor(0xffffff));
        DrawTextCursor(true);
    }
    else if (c >= ' ' && text_window_index < max_chars)
    {
        DrawTextCursor(false);
        WriteAscii(*text_window->Writer(), pos(), c, ToColor(0));
        text_window_index++;
        DrawTextCursor(true);
    }
    layer_manager->Draw(text_window_layer_id);
}

// std::deque<Message> *main_queue;

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

std::shared_ptr<Window> task_b_window;
unsigned int task_b_window_layer_id;
void InitializeTaskBWindow()
{
    task_b_window = std::make_shared<Window>(
        160, 52, screen_config.pixel_format);
    DrawWindow(*task_b_window->Writer(), "TaskB Window");

    task_b_window_layer_id = layer_manager->NewLayer()
                                 .SetWindow(task_b_window)
                                 .SetDraggable(true)
                                 .Move({100, 100})
                                 .ID();
    layer_manager->UpDown(task_b_window_layer_id, std::numeric_limits<int>::max());
}

void TaskB(uint64_t task_id, int64_t data)
{
    printk("TaskB: task_id=%d, data=%d\n", task_id, data);
    char str[128];
    int count = 0;
    while (true)
    {
        count++;
        sprintf(str, "%010d", count);
        FillRectangle(*task_b_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*task_b_window->Writer(), {24, 28}, str, {0, 0, 0});
        layer_manager->Draw(task_b_window_layer_id);
    }
}

void TaskIdle(uint64_t task_id, int64_t data)
{
    printk("TaskIdle: task_id=%lu, data%lx\n", task_id, data);
    while (true)
        __asm__("hlt");
}

extern "C" void
KernelMainNewStack(const FrameBufferConfig &frame_buffer_config_ref,
                   const MemoryMap &memory_map_ref,
                   const acpi::RSDP &acpi_table)
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

    InitializeInterrupt();

    InitializePCI();

    InitializeLayer();
    InitializeMainWindow();
    InitializeTextWindow();
    InitializeTaskBWindow();
    layer_manager->Draw({{0, 0}, ScreenSize()});

    acpi::Initialize(acpi_table);
    InitializeLAPICTimer();

    // timer_manager->AddTimer(Timer(200, 2));
    // timer_manager->AddTimer(Timer(600, -1));

    char str[128];

    const int kTextboxCursorTimer = 1;
    const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
    __asm__("cli");
    timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTimer});
    __asm__("sti");
    bool textbox_cursor_visible = false;

    // memset(&task_b_ctx, 0, sizeof(task_b_ctx));
    // oh I can do cast function to uint64!
    printk("TaskB address:%x\n", reinterpret_cast<uint64_t>(TaskB));
    InitializeTask();

    Task &main_task = task_manager->CurrentTask();
    const uint64_t taskb_id = task_manager->NewTask().InitContext(TaskB, 45).Wakeup().ID();
    task_manager->NewTask().InitContext(TaskIdle, 0xdeadbeef).Wakeup();
    task_manager->NewTask().InitContext(TaskIdle, 0xcafebabe).Wakeup();

    usb::xhci::Initialize();
    InitializeKeyboard();
    InitializeMouse();

    while (1)
    {
        __asm__("cli");
        const auto tick = timer_manager->CurrentTick();
        __asm__("sti");

        sprintf(str, "%010lu", tick);
        FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
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
                timer_manager->AddTimer(Timer(msg->arg.timer.timeout + kTimer05Sec, kTextboxCursorTimer));
                __asm__("sti");
                textbox_cursor_visible = !textbox_cursor_visible;
                DrawTextCursor(textbox_cursor_visible);
                layer_manager->Draw(text_window_layer_id);
            }
            break;
        case Message::kKeyPush:
            InputTextWindow(msg->arg.keyboard.ascii);
            if (msg->arg.keyboard.ascii == 's')
            {
                printk("sleep TaskB: %s\n", task_manager->Sleep(taskb_id).Name());
            }
            else if (msg->arg.keyboard.ascii == 'w')
            {
                printk("wakeup TaskB: %s\n", task_manager->Wakeup(taskb_id).Name());
            }

            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg->type);
        }
    }
}
