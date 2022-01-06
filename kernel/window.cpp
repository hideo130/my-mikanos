#include <algorithm>

#include "error.hpp"
#include "logger.hpp"
#include "window.hpp"
#include "font.hpp"

namespace
{
    const int kCloseButtonWidth = 16;
    const int kCloseButtonHeight = 14;
    const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
        "...............@",
        ".:::::::::::::$@",
        ".:::::::::::::$@",
        ".:::@@::::@@::$@",
        ".::::@@::@@:::$@",
        ".:::::@@@@::::$@",
        ".::::::@@:::::$@",
        ".:::::@@@@::::$@",
        ".::::@@::@@:::$@",
        ".:::@@::::@@::$@",
        ".:::::::::::::$@",
        ".:::::::::::::$@",
        ".$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@",
    };
}

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height}
{
    data_.resize(height);
    for (int y = 0; y < height; y++)
    {
        data_[y].resize(width);
    }

    FrameBufferConfig config{};
    config.frame_buffer = nullptr;
    config.horizontal_resolution = width;
    config.vertical_resolution = height;
    config.pixel_format = shadow_format;

    if (auto err = shadow_buffer_.Initialize(config))
    {
        Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }
}

void Window::DrawTo(FrameBuffer &dst, Vector2D<int> pos, const Rectangle<int> &area)
{
    if (!transparent_color_)
    {
        Rectangle<int> window_area{pos, Size()};
        Rectangle<int> intersection = area & window_area;
        dst.Copy(intersection.pos, shadow_buffer_, {intersection.pos - pos, intersection.size});
        return;
    }

    const auto tc = transparent_color_.value();
    auto &writer = dst.Writer();
    for (int y = std::max(0, 0 - pos.y);
         y < std::min(Height(), writer.Height() - pos.y);
         y++)
    {
        for (int x = std::max(0, 0 - pos.x);
             x < std::min(Width(), writer.Width() - pos.x);
             x++)
        {
            const auto c = At(Vector2D<int>{x, y});
            if (c != tc)
            {
                writer.Write(pos + Vector2D<int>{x, y}, c);
            }
        }
    }
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int> &src)
{
    shadow_buffer_.Move(dst_pos, src);
}

void Window::SetTransparentColor(std::optional<PixelColor> c)
{
    transparent_color_ = c;
}

Window::WindowWriter *Window::Writer()
{
    return &writer_;
}

void Window::Write(Vector2D<int> pos, PixelColor c)
{
    data_[pos.y][pos.x] = c;
    shadow_buffer_.Writer().Write(pos, c);
}

const PixelColor &Window::At(int x, int y) const
{
    return data_[y][x];
}

const PixelColor &Window::At(Vector2D<int> pos) const
{
    return data_[pos.y][pos.x];
}

int Window::Width() const
{
    return width_;
}

int Window::Height() const
{
    return height_;
}

Vector2D<int> Window::Size() const
{
    return Vector2D<int>{width_, height_};
}

ToplevelWindow::ToplevelWindow(int width, int height, PixelFormat shadow_format, const std::string &title)
    : Window{width, height, shadow_format}, title_{title}
{
    DrawWindow(*Writer(), title_.c_str());
}

void ToplevelWindow::Activate()
{
    Window::Activate();
    DrawWindowTitle(*Writer(), title_.c_str(), true);
}

void ToplevelWindow::Deactivate()
{
    Window::Deactivate();
    DrawWindowTitle(*Writer(), title_.c_str(), false);
}

Vector2D<int> ToplevelWindow::InnerSize() const
{
    return Size() - kTopLeftMargin - kBottomRightMargin;
}

void DrawWindow(PixelWriter &writer, const char *title)
{
    auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c)
    {
        FillRectangle(writer, pos, size, ToColor(c));
    };

    const auto win_w = writer.Width();
    const auto win_h = writer.Height();

    const auto tmp1 = 0xc6c6c6;
    fill_rect({0, 0}, {win_w, 1}, tmp1);
    fill_rect({1, 1}, {win_w - 2, 1}, 0xffffff);
    fill_rect({0, 0}, {1, win_h}, tmp1);
    fill_rect({1, 1}, {1, win_h - 2}, 0xffffff);
    fill_rect({win_w - 2, 1}, {1, win_h - 2}, 0x848484);
    fill_rect({win_w - 1, 0}, {1, win_h}, 0x000000);
    fill_rect({2, 2}, {win_w - 4, win_h - 4}, 0xc6c6c6);
    fill_rect({3, 3}, {win_w - 6, 18}, 0x000084);
    fill_rect({1, win_h - 2}, {win_w - 2, 1}, 0x848484);
    fill_rect({0, win_h - 1}, {win_w, 1}, 0x000000);

    WriteString(writer, {24, 4}, title, ToColor(0xffffff));

    for (int y = 0; y < kCloseButtonHeight; ++y)
    {
        for (int x = 0; x < kCloseButtonWidth; ++x)
        {
            PixelColor c = ToColor(0xffffff);
            if (close_button[y][x] == '@')
            {
                c = ToColor(0x000000);
            }
            else if (close_button[y][x] == '$')
            {
                c = ToColor(0x848484);
            }
            else if (close_button[y][x] == ':')
            {
                c = ToColor(0xc6c6c6);
            }
            writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, c);
        }
    }
}

void DrawTextBox(PixelWriter &writer, Vector2D<int> pos, Vector2D<int> size)
{
    auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c)
    {
        FillRectangle(writer, pos, size, ToColor(c));
    };

    // fill main box
    // both boader is 1 pixel, add both size then minus 2
    fill_rect(pos + Vector2D<int>{1, 1}, size - Vector2D<int>{2, 2}, 0xffffff);

    uint32_t tp_l_color = 0x848484;
    uint32_t bm_r_color = 0xc6c6c6;
    // draw border lines
    // top boader
    fill_rect(pos, {size.x, 1}, tp_l_color);
    // left boader
    fill_rect(pos, {1, size.y}, tp_l_color);
    // bottom boader
    fill_rect(pos + Vector2D<int>{0, size.y}, {size.x, 1}, bm_r_color);
    // right boader
    fill_rect(pos + Vector2D<int>{size.x, 0}, Vector2D<int>{1, size.y}, bm_r_color);
}

void DrawWindowTitle(PixelWriter &writer, const char *title, bool activate)
{
    const auto win_w = writer.Width();
    uint32_t bgcolor = 0x848484;
    if (activate)
    {
        bgcolor = 0x000084;
    }

    FillRectangle(writer, {3, 3}, {win_w - 6, 18}, ToColor(bgcolor));
    WriteString(writer, {24, 4}, title, ToColor(0xffffff));

    for (int y = 0; y < kCloseButtonHeight; y++)
    {
        for (int x = 0; x < kCloseButtonWidth; x++)
        {
            PixelColor c = ToColor(0xffffff);
            if (close_button[y][x] == '0')
            {
                c = ToColor(0x000000);
            }
            else if (close_button[y][x] == '$')
            {
                c = ToColor(0x848484);
            }
            else if (close_button[y][x] == ':')
            {
                c = ToColor(0xc6c6c6);
            }
            writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, c);
        }
    }
}
