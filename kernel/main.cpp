#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <array>
#include <deque>
#include <numeric>
#include <vector>

#include "asmfunc.h"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "layer.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "mouse.hpp"
#include "queue.hpp"
#include "segment.hpp"
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

std::deque<Message> *main_queue;

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void
KernelMainNewStack(const FrameBufferConfig &frame_buffer_config_ref,
                   const MemoryMap &memory_map_ref,
                   const acpi::RSDP& acpi_table)
{
    MemoryMap memory_map{memory_map_ref};

    InitializedGraphics(frame_buffer_config_ref);
    InitializeConsole();
    // WriteAscii(*pixel_writer, 58, 50, 'a', {0, 0, 0});
    // WriteString(*pixel_writer, 100, 66, "Hello World!", {0, 0, 255});
    // char buf[128];
    // sprintf(buf, "1+2=%d", 1 + 2);
    // WriteString(*pixel_writer, 100, 300, buf, {0, 0, 255});

    printk("Welcom to MyMikcanos!\n");
    SetLogLevel(kError);


    InitializeSegmentation();
    SetupIdentityPageTable();

    InitializeMemoryManager(memory_map);
    printk("memory_map: %p\n", &memory_map);

    ::main_queue = new std::deque<Message>(32);
    InitializeInterrupt(main_queue);

    InitializePCI();
    usb::xhci::Initialize();

    InitializeLayer();
    InitializeMainWindow();
    InitializeMouse();
    layer_manager->Draw({{0, 0}, ScreenSize()});

    acpi::Initialize(acpi_table);
    InitializeLAPICTimer(*main_queue);

    timer_manager->AddTimer(Timer(200, 2));
    timer_manager->AddTimer(Timer(600, -1));



    char str[128];

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
        if (main_queue->size() == 0)
        {
            __asm__("sti\n\thlt");
            continue;
        }

        Message msg = main_queue->front();
        main_queue->pop_front();
        __asm__("sti");

        switch (msg.type)
        {
        case Message::kInterruptXHCI:
            usb::xhci::ProcessEvents();
            break;
        case Message::kTimerTimeout:
            printk("Timer: timeout = %lu, value = %d\n",
                   msg.arg.timer.timeout, msg.arg.timer.value);
            if (msg.arg.timer.value > 0)
            {
                timer_manager->AddTimer(Timer(msg.arg.timer.timeout + 100,
                                              msg.arg.timer.value + 1));                                              
            }
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}
