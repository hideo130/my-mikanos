#include "graphics.hpp"

void WriteAscii(PixelWriter&, int, int, char, const PixelColor &);
void WriteString(PixelWriter &writer, Vector2D<int> pos, const char *s, const PixelColor &color);
void WriteString(PixelWriter &writer, int x, int y, const char *s, const PixelColor &color);