// SPDX-License-Identifier: Apache-2.0
#include "ui_widgets.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ui_font.h"

enum { BUTTON_GAP = 2 };        /* Space between neighbouring buttons. */

bool ui_rect_contains(ui_rect_t rect, int32_t x, int32_t y)
{
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

bool ui_pointer_inside(const ui_pointer_t *pointer, ui_rect_t rect)
{
    return pointer && pointer->down && ui_rect_contains(rect, pointer->x, pointer->y);
}

/* Where rectangles go: NULL means Raylib's own target, otherwise a raw RGB565 surface. */
static uint16_t *s_surface;
static size_t s_surface_stride;

void ui_surface_set(uint16_t *pixels, size_t stride)
{
    s_surface = pixels;
    s_surface_stride = stride;
}

static uint16_t to_rgb565(Color c)
{
    return (uint16_t)(((c.r & 0xF8U) << 8) | ((c.g & 0xFCU) << 3) | (c.b >> 3));
}

void ui_rect_draw(int x, int y, int w, int h, Color color)
{
    if (!s_surface) {
        DrawRectangle(x, y, w, h, color);
        return;
    }
    const int x0 = x < 0 ? 0 : x;
    const int y0 = y < 0 ? 0 : y;
    const int x1 = x + w > UI_SURFACE_SIZE ? UI_SURFACE_SIZE : x + w;
    const int y1 = y + h > UI_SURFACE_SIZE ? UI_SURFACE_SIZE : y + h;
    const uint16_t value = to_rgb565(color);
    for (int row = y0; row < y1; ++row) {
        uint16_t *line = s_surface + (size_t)row * s_surface_stride;
        for (int col = x0; col < x1; ++col) line[col] = value;
    }
}

void ui_fill(ui_rect_t rect, Color color)
{
    ui_rect_draw(rect.x, rect.y, rect.w, rect.h, color);
}

void ui_outline(ui_rect_t rect, int thickness, Color color)
{
    ui_rect_draw(rect.x, rect.y, rect.w, thickness, color);
    ui_rect_draw(rect.x, rect.y + rect.h - thickness, rect.w, thickness, color);
    ui_rect_draw(rect.x, rect.y, thickness, rect.h, color);
    ui_rect_draw(rect.x + rect.w - thickness, rect.y, thickness, rect.h, color);
}

void ui_button(ui_rect_t rect, const char *label, int scale, ui_button_style_t style,
               const ui_pointer_t *pointer)
{
    Color fill = UI_COLOR_KEY;
    Color text = UI_COLOR_TEXT;
    switch (style) {
    case UI_BUTTON_NORMAL: break;
    case UI_BUTTON_FUNCTION: fill = UI_COLOR_KEY_FN; break;
    case UI_BUTTON_SELECTED: fill = UI_COLOR_ACCENT; text = UI_COLOR_ON_ACCENT; break;
    case UI_BUTTON_DISABLED: fill = UI_COLOR_PANEL; text = UI_COLOR_MUTED; break;
    }
    if (style != UI_BUTTON_DISABLED && ui_pointer_inside(pointer, rect)) {
        fill = UI_COLOR_KEY_DOWN;
        text = UI_COLOR_ON_ACCENT;
    }
    const ui_rect_t body = {
        (int16_t)(rect.x + BUTTON_GAP), (int16_t)(rect.y + BUTTON_GAP),
        (int16_t)(rect.w - 2 * BUTTON_GAP), (int16_t)(rect.h - 2 * BUTTON_GAP),
    };
    ui_fill(body, fill);
    ui_text_draw_center(label, body.x + body.w / 2, body.y + (body.h - ui_text_height(scale)) / 2,
                        scale, text);
}

void ui_lines_init(ui_lines_t *lines, int count)
{
    memset(lines, 0, sizeof(*lines));
    lines->count = (uint8_t)(count < UI_LINES_MAX ? count : UI_LINES_MAX);
}

void ui_lines_printf(ui_lines_t *lines, int index, const char *format, ...)
{
    if ((unsigned)index >= UI_LINES_MAX) return;
    va_list args;
    va_start(args, format);
    (void)vsnprintf(lines->text[index], UI_LINE_MAX, format, args);
    va_end(args);
}

const char *ui_lines_get(const ui_lines_t *lines, int index)
{
    return (unsigned)index < lines->count ? lines->text[index] : NULL;
}
