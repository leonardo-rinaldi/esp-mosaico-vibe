// SPDX-License-Identifier: Apache-2.0
#include "ui_widgets.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ui_font.h"

enum { BUTTON_GAP = 2, FULL_BRIGHTNESS = 100 };

static int s_brightness = FULL_BRIGHTNESS;

void ui_dim_set(int percent)
{
    s_brightness = percent < 0 ? 0 : (percent > FULL_BRIGHTNESS ? FULL_BRIGHTNESS : percent);
}

Color ui_dim_color(Color color)
{
    if (s_brightness == FULL_BRIGHTNESS) return color;
    color.r = (unsigned char)(color.r * s_brightness / FULL_BRIGHTNESS);
    color.g = (unsigned char)(color.g * s_brightness / FULL_BRIGHTNESS);
    color.b = (unsigned char)(color.b * s_brightness / FULL_BRIGHTNESS);
    return color;
}        /* Space between neighbouring buttons. */

bool ui_rect_contains(ui_rect_t rect, int32_t x, int32_t y)
{
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

bool ui_pointer_inside(const ui_pointer_t *pointer, ui_rect_t rect)
{
    return pointer && pointer->down && ui_rect_contains(rect, pointer->x, pointer->y);
}

void ui_fill(ui_rect_t rect, Color color)
{
    DrawRectangle(rect.x, rect.y, rect.w, rect.h, color);
}

void ui_outline(ui_rect_t rect, int thickness, Color color)
{
    DrawRectangle(rect.x, rect.y, rect.w, thickness, color);
    DrawRectangle(rect.x, rect.y + rect.h - thickness, rect.w, thickness, color);
    DrawRectangle(rect.x, rect.y, thickness, rect.h, color);
    DrawRectangle(rect.x + rect.w - thickness, rect.y, thickness, rect.h, color);
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
