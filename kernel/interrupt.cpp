#include <cstdint>
#include <array>

#include "asmfunc.h"
#include "font.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "segment.hpp"
#include "task.hpp"
#include "timer.hpp"

void NotifyEndOfInterrupt()
{
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t *>(0xfee000b0);
    *end_of_interrupt = 0;
}

std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(InterruptDescriptor &desc, InterruptDescriptorAttribute attr,
                 uint64_t offset, uint16_t segment_selector)
{
    desc.attr = attr;
    desc.offser_low = offset & 0xffffu;
    desc.offset_middle = (offset >> 16) & 0xffffu;
    desc.offset_high = offset >> 32;
    desc.segment_selector = segment_selector;
}

namespace
{
    __attribute__((interrupt)) void IntHandlerXHCI(InterruptFrame *frame)
    {
        task_manager->SendMessage(1, Message{Message::kInterruptXHCI});
        NotifyEndOfInterrupt();
    }

    __attribute__((interrupt)) void IntHandlerLAPICTimer(InterruptFrame *frame)
    {
        LAPICTimerOnInterrupt();
    }
    void PrintHex(uint64_t value, int width, Vector2D<int> pos)
    {
        for (int i = 0; i < width; i++)
        {
            int x = (value >> 4 * (width - i - 1)) & 0xfu;
            if (x >= 10)
            {
                x += 'a' - 10;
            }
            else
            {
                x += '0';
            }
            WriteAscii(*screen_writer, pos + Vector2D<int>{8 * i, 0}, x, {0, 0, 0});
        }
    }

#define FaultHandlerWithError(fault_name)                                                              \
    __attribute__((interrupt)) void IntHandler##fault_name(InterruptFrame *frame, uint64_t error_code) \
    {                                                                                                  \
        PrintFrame(frame, "#" #fault_name);                                                            \
        WriteWtring(*screen_writer, {500 + 8 * 4, 16 * 4});                                            \
        PrintHex(error_code, 16, {500 + 8 * 4, 16 * 4});                                               \
        while (true)                                                                                   \
            __asm__("hlt");                                                                            \
    }

#define FaultHandlerNoError(fault_name)                                             \
    __attribute__(((interrupt))) void IntHandler##fault_name(InterruptFrame *frame) \
    {                                                                               \
        PrintFrame(frame, "#", #fault_name);                                        \
        while (true)                                                                \
            __asm__("hlt");                                                         \
    }

    FaultHandlerNoError(DE);
    FaultHandlerNoError(DE);
    FaultHandlerNoError(DB);
    FaultHandlerNoError(BP);
    FaultHandlerNoError(OF);
    FaultHandlerNoError(BR);
    FaultHandlerNoError(UD);
    FaultHandlerNoError(NM);
    FaultHandlerWithError(DF);
    FaultHandlerWithError(TS);
    FaultHandlerWithError(NP);
    FaultHandlerWithError(SS);
    FaultHandlerWithError(GP);
    FaultHandlerWithError(PF);
    FaultHandlerNoError(MF);
    FaultHandlerWithError(AC);
    FaultHandlerNoError(MC);
    FaultHandlerNoError(XM);
    FaultHandlerNoError(VE);

}

void InitializeInterrupt()
{
    SetIDTEntry(idt[InterruptVector::kXHCI],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI),
                kKernelCS);
    SetIDTEntry(idt[InterruptVector::kLAPICTimer],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
                kKernelCS);
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
