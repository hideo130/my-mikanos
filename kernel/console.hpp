#pragma once

#include "graphics.hpp"
#include "window.hpp"

class Console
{
public:
    static const int kRows = 30, kColumns = 80;
    Console(PixelWriter &writer,
            const PixelColor &fg_color, const PixelColor &bg_color);
    Console(const PixelColor &fg_color, const PixelColor &bg_color);
    unsigned int LayerID();
    void PutString(const char *s);
    void SetLayerID(unsigned int layer_id);
    void SetWindow(const std::shared_ptr<Window> &window);
    void SetWriter(PixelWriter *writer);
    void Refresh();

private:
    void Newline();

    PixelWriter *writer_;
    std::shared_ptr<Window> window_;
    const PixelColor fg_color_, bg_color_;
    char buffer_[kRows][kColumns + 1];
    int cursor_row_, cursor_column_;
    unsigned int layer_id_;
};