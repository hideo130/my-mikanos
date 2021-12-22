#include "graphics.hpp"

void RGBResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor &c)
{
    auto p = PixelAt(pos.x, pos.y);
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
}

void BGRResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor &c)
{
    auto p = PixelAt(pos.x, pos.y);
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
}

void FillRectangle(PixelWriter &writer, const Vector2D<int> &pos,
                   const Vector2D<int> &size, const PixelColor &c)
{
    for (int dy = 0; dy < size.y; dy++)
    {
        for (int dx = 0; dx < size.x; dx++)
        {
            writer.Write(pos + Vector2D<int>{dx, dy}, c);
        }
    }
}

void DrawRectangle(PixelWriter &writer, const Vector2D<int> &pos,
                   const Vector2D<int> &size, const PixelColor &c)
{
    for (int dx = 0; dx < size.x; dx++)
    {
        // write width edge
        writer.Write(Vector2D<int>{pos.x + dx, pos.y}, c);
        writer.Write(Vector2D<int>{pos.x + dx, pos.y + size.y - 1}, c);
    }

    for (int dy = 1; dy < size.y - 1; dy++)
    {
        // write height edge
        writer.Write(Vector2D<int>{pos.x, pos.y + dy}, c);
        writer.Write(Vector2D<int>{pos.x + size.x - 1, pos.y + dy}, c);
    }
}

void DrawDesktop(PixelWriter &writer)
{
    const auto width = writer.Width();
    const auto height = writer.Height();

    FillRectangle(writer, {0, 0}, {width, height - 50}, kDesktopBGColor);
    FillRectangle(writer, {0, height - 50}, {width, 50}, {1, 8, 17});
    FillRectangle(writer, {0, height - 50}, {width / 5, 50}, {80, 80, 80});
    DrawRectangle(writer, {10, height - 40}, {30, 30}, {160, 160, 160});
}

FrameBufferConfig screen_config;
PixelWriter *screen_writer;

Vector2D<int> ScreenSize()
{
    return {
        static_cast<int>(screen_config.horizontal_resolution),
        static_cast<int>(screen_config.vertical_resolution)};
}

namespace
{
    char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
}

void InitializedGraphics(const FrameBufferConfig &screen_config)
{
    ::screen_config = screen_config;
    switch (screen_config.pixel_format)
    {
    case kPixelRGBResv8BitPerColor:
        ::screen_writer = new (pixel_writer_buf)
            RGBResv8BitPerColorPixelWriter{screen_config};
        break;
    case kPixelBGRResv8BitPerColor:
        ::screen_writer = new (pixel_writer_buf)
            BGRResv8BitPerColorPixelWriter{screen_config};
        break;
    default:
        exit(1);
    }

    DrawDesktop(*screen_writer);
}