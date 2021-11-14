#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"

void *operator new(std::size_t size, void *buf)
{
    return buf;
}

void operator delete(void *obj) noexcept {}

char console_buf[sizeof(Console)];
Console *console;

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter *pixel_writer;

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

extern "C" void
KernelMain(const FrameBufferConfig &frame_buffer_config)
{
    switch (frame_buffer_config.pixels_format)
    {
    case kPixelRGBResv8BitPerColor:
        pixel_writer = new (pixel_writer_buf)
            RGBResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    case kPixelBGRResv8BitPerColor:
        pixel_writer = new (pixel_writer_buf)
            BGRResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    default:
        break;
    }

    for (int x = 0; x < frame_buffer_config.horizontal_resolution; x++)
    {
        for (int y = 0; y < frame_buffer_config.vertical_resolution; y++)
        {
            pixel_writer->Write(x, y, {255, 255, 255});
        }
    }

    for (int x = 0; x < 200; x++)
    {
        for (int y = 0; y < 200; y++)
        {
            pixel_writer->Write(100 + x, 100 + y, {0, 255, 0});
        }
    }

    for (int i = 0; i < 100; i += 8)
    {
        WriteAscii(*pixel_writer, 50 + i, 50, 'A' + i, {0, 0, 0});
    }

    // WriteAscii(*pixel_writer, 58, 50, 'a', {0, 0, 0});
    WriteString(*pixel_writer, 100, 66, "Hello World!", {0, 0, 255});
    char buf[128];
    sprintf(buf, "1+2=%d", 1 + 2);
    WriteString(*pixel_writer, 100, 300, buf, {0, 0, 255});
    console = new (console_buf) Console{*pixel_writer, {0, 0, 0}, {255, 255, 255}};

    for (int i = 0; i < 16; i++)
    {
        printk("printk%d\n", i);
    }

    while (1)
    {
        __asm__("hlt");
    }
}
