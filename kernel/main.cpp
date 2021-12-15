#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <array>
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

    StartLAPICTimer();
    console->PutString(s);
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();
    sprintf(s, "[%9d]", elapsed);
    console->PutString(s);
    return result;
}

void SwitchEhci2Xhci(const pci::Device &xhc_dev)
{
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::num_device; i++)
    {
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) && /* EHCI */
            0x8086 == pci::ReadVendorId(pci::devices[i]))
        {
            intel_ehc_exist = true;
            break;
        }
    }
    if (!intel_ehc_exist)
    {
        return;
    }

    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc); // USB3PRM
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports);          // USB3_PSSEN
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4);  // XUSB2PRM
    pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports);           // XUSB2PR
    Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n", superspeed_ports, ehci2xhci_ports);
}

unsigned int mouse_layer_id;
Vector2D<int> screen_size;
Vector2D<int> mouse_position;

void MouseObserver(uint8_t buttons, int8_t displacement_x, int8_t displacement_y)
{
    static unsigned int mouse_drag_layer_id = 0;
    static uint8_t previous_buttons = 0;

    const auto oldpos = mouse_position;
    auto newpos = mouse_position + Vector2D<int>{displacement_x, displacement_y};
    newpos = ElementMin(newpos, screen_size + Vector2D<int>{-1, -1});
    mouse_position = ElementMax(newpos, {0, 0});

    const auto posdiff = mouse_position - oldpos;

    layer_manager->Move(mouse_layer_id, mouse_position);

    const bool previous_left_pressed = (previous_buttons & 0x01);
    const bool left_pressed = (buttons & 0x01);

    if (!previous_left_pressed && left_pressed)
    {
        auto layer = layer_manager->FindLayerByPosition(mouse_position, mouse_layer_id);
        if (layer && layer->IsDraggable())
        {
            mouse_drag_layer_id = layer->ID();
        }
    }
    else if (previous_left_pressed && left_pressed)
    {
        if (mouse_drag_layer_id > 0)
        {
            layer_manager->MoveRelative(mouse_drag_layer_id, posdiff);
        }
    }
    else if (previous_left_pressed && !left_pressed)
    {
        mouse_drag_layer_id = 0;
    }
    previous_buttons = buttons;

    StartLAPICTimer();
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();
    printk("mouseObserver :elapsed %u\n", elapsed);
}

extern "C" void __cxa_pure_virtual()
{
    while (1)
        __asm__("hlt");
}

usb::xhci::Controller *xhc;
struct Message
{
    enum Type
    {
        kInterruptXHCI,
    } type;
};

ArrayQueue<Message> *main_queue;

__attribute__((interrupt)) void IntHandlerXHCI(InterruptFrame *frame)
{
    main_queue->Push(Message{Message::kInterruptXHCI});
    NotifyEndOfInterrupt();
}

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void
KernelMainNewStack(const FrameBufferConfig &frame_buffer_config_ref,
                   const MemoryMap &memory_map_ref)
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

    InitializeLAPICTimer();

    InitializeSegmentation();
    SetupIdentityPageTable();

    InitializeMemoryManager(memory_map);
    printk("memory_map: %p\n", &memory_map);

    std::array<Message, 32> main_queue_data;
    ArrayQueue<Message> main_queue{main_queue_data};
    ::main_queue = &main_queue;

    auto err = pci::ScanAllBus();
    Log(kDebug, "ScanAllBus: %s\n", err.Name());
    for (int i = 0; i < pci::num_device; i++)
    {
        const auto &dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n",
            dev.bus, dev.device, dev.function,
            vendor_id, class_code, dev.header_type);
    }

    pci::Device *xhc_dev = nullptr;
    for (int i = 0; i < pci::num_device; i++)
    {
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u))
        {
            xhc_dev = &pci::devices[i];
            if (0x8086 == pci::ReadVendorId(*xhc_dev))
            {
                break;
            }
        }
    }
    if (xhc_dev)
    {
        Log(kInfo, "xHC has been found: %d.%d.%d\n", xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI), kKernelCS);
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

    const uint8_t bsp_local_apic_id = *reinterpret_cast<const uint32_t *>(0xfee00020) >> 24;
    pci::ConfigureMSIFixedDestination(
        *xhc_dev, bsp_local_apic_id,
        pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
        InterruptVector::kXHCI, 0);

    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);

    usb::xhci::Controller xhc{xhc_mmio_base};

    if (0x8086 == pci::ReadVendorId(*xhc_dev))
    {
        SwitchEhci2Xhci(*xhc_dev);
    }
    {
        auto err = xhc.Initialize();
        Log(kDebug, "xhc.Initialize: %s\n", err.Name());
    }

    Log(kInfo, "xHC starting\n");
    xhc.Run();
    ::xhc = &xhc;

    usb::HIDMouseDriver::default_observer = MouseObserver;

    for (int i = 1; i <= xhc.MaxPorts(); i++)
    {
        auto port = xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

        if (port.IsConnected())
        {
            if (auto err = ConfigurePort(xhc, port))
            {
                Log(kError, "failed to configure port %s at %s:%d\n",
                    err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    const int kFrameWidth = screen_config.horizontal_resolution;
    const int kFrameHeight = screen_config.vertical_resolution;
    const auto kFrameFormat = screen_config.pixel_format;

    screen_size.x = screen_config.horizontal_resolution;
    screen_size.y = screen_config.vertical_resolution;

    // generate two layer
    auto bgwindow = std::make_shared<Window>(kFrameWidth, kFrameHeight, kFrameFormat);
    auto bgwriter = bgwindow->Writer();

    DrawDesktop(*bgwriter);
    // console->SetWindow(bgwindow);

    auto mouse_window = std::make_shared<Window>(
        kMouseCursorWidth, kMouseCursorHeight, kFrameFormat);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});

    FrameBuffer screen;
    if (auto err = screen.Initialize(screen_config))
    {
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }

    auto main_window = std::make_shared<Window>(
        160, 52, screen_config.pixel_format);
    DrawWindow(*main_window->Writer(), "Hello Window");
    // WriteString(*main_window->Writer(), {24, 28}, "Welcomet to ", {0, 0, 0});
    // WriteString(*main_window->Writer(), {24, 44}, " MikanOS world!", {0, 0, 0});

    auto console_window = std::make_shared<Window>(
        Console::kColumns * 8, Console::kRows * 16, screen_config.pixel_format);
    console->SetWindow(console_window);

    layer_manager = new LayerManager;
    layer_manager->SetWriter(&screen);

    auto bglayer_id = layer_manager->NewLayer()
                          .SetWindow(bgwindow)
                          .Move({0, 0})
                          .ID();

    mouse_layer_id = layer_manager->NewLayer()
                         .SetWindow(mouse_window)
                         .Move({200, 200})
                         .ID();

    auto main_window_layer_id = layer_manager->NewLayer()
                                    .SetWindow(main_window)
                                    .SetDraggable(true)
                                    .Move({300, 100})
                                    .ID();
    console->SetLayerID(
        layer_manager->NewLayer()
            .SetWindow(console_window)
            .Move({0, 0})
            .ID());

    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(console->LayerID(), 1);
    layer_manager->UpDown(main_window_layer_id, 2);
    layer_manager->UpDown(mouse_layer_id, 3);
    layer_manager->Draw({{0, 0}, screen_size});

    char str[128];
    unsigned int count = 0;

    while (1)
    {
        ++count;
        sprintf(str, "%010u", count);
        FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
        layer_manager->Draw(main_window_layer_id);

        __asm__("cli");
        if (main_queue.Count() == 0)
        {
            __asm__("sti");
            continue;
        }

        Message msg = main_queue.Front();
        main_queue.Pop();
        __asm__("sti");

        switch (msg.type)
        {
        case Message::kInterruptXHCI:
            while (xhc.PrimaryEventRing()->HasFront())
            {
                if (auto err = ProcessEvent(xhc))
                {
                    Log(kError, "Error while ProcessEvent: %s at %s:%d\n", err.Name(), err.File(), err.Line());
                }
            }
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}
