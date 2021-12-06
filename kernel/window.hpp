#pragma once

#include <optional>
#include <vector>

#include "frame_buffer.hpp"
#include "graphics.hpp"

class Window
{
public:
    class WindowWriter : public PixelWriter
    {
    public:
        WindowWriter(Window &window) : window_{window} {}
        /** @brief 指定された位置に指定された色を描く */
        virtual void Write(Vector2D<int> pos, const PixelColor &c) override
        {
            window_.Write(pos, c);
        }
        /** @brief Width は関連付けられた Window の横幅をピクセル単位で返す。 */
        virtual int Width() const override { return window_.Width(); }
        /** @brief Height は関連付けられた Window の高さをピクセル単位で返す。 */
        virtual int Height() const override { return window_.Height(); }

    private:
        Window &window_;
    };

    Window(int width, int height, PixelFormat shadow_format);
    ~Window() = default;
    Window(const Window &rhs) = delete;
    Window &operator=(const Window &rhs) = delete;

    void DrawTo(PixelWriter &writer, Vector2D<int> position);
    void DrawTo(FrameBuffer &screen, Vector2D<int> position);
    void Move(Vector2D<int> dst_pos, const Rectangle<int> &src);
    void SetTransparentColor(std::optional<PixelColor> c);
    WindowWriter *Writer();

    void Write(Vector2D<int> pos, PixelColor c);
    const PixelColor &At(int x, int y) const;
    const PixelColor &At(Vector2D<int> pos) const;

    int Width() const;
    int Height() const;

private:
    int width_, height_;
    std::vector<std::vector<PixelColor>> data_{};
    WindowWriter writer_{*this};
    std::optional<PixelColor> transparent_color_{std::nullopt};

    FrameBuffer shadow_buffer_{};
};

void DrawWindow(PixelWriter &writer, const char *title);