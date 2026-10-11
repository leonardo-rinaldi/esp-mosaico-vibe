// SPDX-License-Identifier: Apache-2.0
/* Text drawing for the bench assistant.
 *
 * The engine's built-in font draws only digits, letters (always as capitals) and
 * "/ - : .". A measuring tool cannot live with that: it needs "Ω", "µ", "²", "±",
 * "%", the decimal comma, and a lowercase "m" that differs from the capital "M"
 * (milli versus mega). This module draws those itself, as 5x7 bitmaps on the same
 * 6-pixel cell grid, and hands every other character to the engine.
 *
 * Text is UTF-8. Write interface words in capitals; the only lowercase letters
 * with their own shape are the SI prefixes m, k, p and n. Any other lowercase
 * letter is drawn as a capital, and characters with no glyph leave a blank cell. */
#pragma once
#include "mosaico_raylib_fast.h"
#include "ui_dim.h"

enum { UI_FONT_CELL = 6, UI_FONT_HEIGHT = 7 };  /* Pixels per cell and glyph row count at scale 1. */

int ui_text_width(const char *utf8, int scale);
int ui_text_height(int scale);
void ui_text_draw(const char *utf8, int x, int y, int scale, Color color);
void ui_text_draw_right(const char *utf8, int right, int y, int scale, Color color);
void ui_text_draw_center(const char *utf8, int center, int y, int scale, Color color);
