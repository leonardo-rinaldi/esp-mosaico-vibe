// SPDX-License-Identifier: Apache-2.0
#include "ui_font.h"
#include <stddef.h>
#include <stdint.h>

enum {
    ENGINE_FONT_UNIT = 8,           /* The engine draws at scale = font_size / 8. */
    GLYPH_COLUMNS = 5,
    UTF8_CONTINUATION_MASK = 0xC0,
    UTF8_CONTINUATION = 0x80,
};

typedef struct {
    uint32_t code;
    uint8_t rows[UI_FONT_HEIGHT];   /* Bit 4 is the leftmost pixel. */
} glyph_t;

static const glyph_t s_glyphs[] = {
    {',', {0, 0, 0, 0, 6, 6, 4}},
    {'%', {24, 25, 2, 4, 8, 19, 3}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},
    {'<', {2, 4, 8, 16, 8, 4, 2}},
    {'m', {0, 0, 26, 21, 21, 21, 21}},
    {'k', {16, 16, 18, 20, 24, 20, 18}},
    {'p', {0, 0, 30, 17, 30, 16, 16}},
    {'n', {0, 0, 22, 25, 17, 17, 17}},
    {0x03A9, {14, 17, 17, 17, 10, 10, 27}},     /* Ω Greek capital omega */
    {0x2126, {14, 17, 17, 17, 10, 10, 27}},     /* Ω ohm sign */
    {0x00B5, {0, 17, 17, 17, 19, 29, 16}},      /* µ micro sign */
    {0x03BC, {0, 17, 17, 17, 19, 29, 16}},      /* μ Greek small mu */
    {0x00B2, {6, 9, 2, 4, 15, 0, 0}},           /* ² */
    {0x00B0, {6, 9, 9, 6, 0, 0, 0}},            /* ° */
    {0x00B1, {4, 4, 31, 4, 4, 0, 31}},          /* ± */
};

static uint32_t utf8_next(const char **cursor)
{
    const unsigned char *p = (const unsigned char *)*cursor;
    uint32_t code = *p++;
    int extra = 0;
    if (code >= 0xF0) { code &= 0x07; extra = 3; }
    else if (code >= 0xE0) { code &= 0x0F; extra = 2; }
    else if (code >= 0xC0) { code &= 0x1F; extra = 1; }
    else if (code >= UTF8_CONTINUATION) code = 0;   /* Stray continuation byte: blank cell. */
    while (extra-- > 0 && (*p & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION)
        code = (code << 6) | (*p++ & 0x3F);
    *cursor = (const char *)p;
    return code;
}

static const glyph_t *find_glyph(uint32_t code)
{
    for (size_t i = 0; i < sizeof(s_glyphs) / sizeof(s_glyphs[0]); ++i)
        if (s_glyphs[i].code == code) return &s_glyphs[i];
    return NULL;
}

/* Characters the engine font draws. */
static bool engine_has(uint32_t code)
{
    return (code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z') ||
           (code >= 'a' && code <= 'z') || code == '/' || code == '-' ||
           code == ':' || code == '.';
}

static void draw_glyph(const glyph_t *glyph, int x, int y, int scale, Color color)
{
    for (int row = 0; row < UI_FONT_HEIGHT; ++row) {
        int col = 0;
        while (col < GLYPH_COLUMNS) {
            while (col < GLYPH_COLUMNS && !(glyph->rows[row] & (1U << (GLYPH_COLUMNS - 1 - col)))) ++col;
            const int start = col;
            while (col < GLYPH_COLUMNS && (glyph->rows[row] & (1U << (GLYPH_COLUMNS - 1 - col)))) ++col;
            if (col > start)
                DrawRectangle(x + start * scale, y + row * scale, (col - start) * scale, scale, color);
        }
    }
}

int ui_text_width(const char *utf8, int scale)
{
    if (!utf8 || !*utf8 || scale < 1) return 0;
    int cells = 0;
    while (*utf8) {
        (void)utf8_next(&utf8);
        ++cells;
    }
    return cells * UI_FONT_CELL * scale - scale;
}

int ui_text_height(int scale)
{
    return UI_FONT_HEIGHT * (scale < 1 ? 1 : scale);
}

void ui_text_draw(const char *utf8, int x, int y, int scale, Color color)
{
    if (!utf8 || scale < 1) return;
    char single[2] = {0, 0};
    while (*utf8) {
        const uint32_t code = utf8_next(&utf8);
        const glyph_t *glyph = find_glyph(code);
        if (glyph) {
            draw_glyph(glyph, x, y, scale, color);
        } else if (engine_has(code)) {
            single[0] = (char)code;
            DrawText(single, x, y, ENGINE_FONT_UNIT * scale, color);
        }
        x += UI_FONT_CELL * scale;
    }
}

void ui_text_draw_right(const char *utf8, int right, int y, int scale, Color color)
{
    ui_text_draw(utf8, right - ui_text_width(utf8, scale), y, scale, color);
}

void ui_text_draw_center(const char *utf8, int center, int y, int scale, Color color)
{
    ui_text_draw(utf8, center - ui_text_width(utf8, scale) / 2, y, scale, color);
}
