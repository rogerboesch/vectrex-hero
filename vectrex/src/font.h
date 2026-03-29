//
// font.h — Custom VLC font for H.E.R.O.
//

#ifndef FONT_H
#define FONT_H

#include <vectrex.h>

const int8_t *font_glyph(char c);
void draw_text(int8_t y, int8_t x, const char *str, uint8_t scale, uint8_t advance);

#endif
