#include "mouse.hpp"

namespace
{
    const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
        "@              ",
        "@@             ",
        "@.@            ",
        "@..@           ",
        "@...@          ",
        "@....@         ",
        "@.....@        ",
        "@......@       ",
        "@.......@      ",
        "@........@     ",
        "@.........@    ",
        "@..........@   ",
        "@...........@  ",
        "@............@ ",
        "@......@@@@@@@@",
        "@......@       ",
        "@....@@.@      ",
        "@...@ @.@      ",
        "@..@   @.@     ",
        "@.@    @.@     ",
        "@@      @.@    ",
        "@       @.@    ",
        "         @.@   ",
        "         @@@   ",
    };
}

void DrawMouseCursor(PixelWriter *pixel_writer, Vector2D<int> position)
{
    for (int dy = 0; dy < kMouseCursorHeight; dy++)
    {
        for (int dx = 0; dx < kMouseCursorWidth; dx++)
        {
            PixelColor c;
            if (mouse_cursor_shape[dy][dx] == '@')
            {
                c = {0, 0, 0};
            }
            else if (mouse_cursor_shape[dy][dx] == '.')
            {
                c = {255, 255, 255};
            }
            else
            {
                c = kMouseTransparentColor;
            }
            pixel_writer->Write(position + Vector2D<int>{dx, dy}, c);
        }
    }
}