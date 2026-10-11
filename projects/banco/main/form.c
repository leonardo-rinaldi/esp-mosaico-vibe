// SPDX-License-Identifier: Apache-2.0
#include "form.h"
#include <math.h>
#include <stdio.h>
#include "ui_font.h"

enum { SIGNIFICANT_DIGITS = 4 };

double form_round_significant(double value, int digits)
{
    if (!(fabs(value) > 0.0) || !isfinite(value)) return value;
    const double scale = pow(10.0, digits - 1 - (int)floor(log10(fabs(value))));
    return round(value * scale) / scale;
}

enum { FIELD_LABEL_X = 16, FIELD_VALUE_SCALE = 3, FIELD_LABEL_SCALE = 2, SCREEN_WIDTH = 480 };

ui_rect_t form_field_rect(int top, int index)
{
    return (ui_rect_t){6, (int16_t)(top + index * FORM_FIELD_PITCH), 468, FORM_FIELD_HEIGHT};
}

int form_field_hit(int top, int count, int32_t x, int32_t y)
{
    for (int i = 0; i < count; ++i)
        if (ui_rect_contains(form_field_rect(top, i), x, y)) return i;
    return -1;
}

void form_draw_field(ui_rect_t rect, const char *label, const char *value, const char *unit, bool selected)
{
    ui_fill(rect, UI_COLOR_PANEL);
    if (selected) ui_outline(rect, 3, UI_COLOR_ACCENT);
    const int center = rect.y + rect.h / 2;
    ui_text_draw(label, FIELD_LABEL_X, center - ui_text_height(FIELD_LABEL_SCALE) / 2, FIELD_LABEL_SCALE,
                 UI_COLOR_MUTED);
    const bool empty = !value || !*value;
    ui_text_draw_right(empty ? "-" : value, FORM_VALUE_RIGHT, center - ui_text_height(FIELD_VALUE_SCALE) / 2,
                       FIELD_VALUE_SCALE, empty ? UI_COLOR_MUTED : UI_COLOR_TEXT);
    if (unit && *unit)
        ui_text_draw(unit, FORM_UNIT_X, center - ui_text_height(FIELD_LABEL_SCALE) / 2, FIELD_LABEL_SCALE,
                     UI_COLOR_MUTED);
}

void form_format_quantity(calc_qty_t qty, double base_value, char *out, size_t size)
{
    if (!isfinite(base_value)) {
        (void)snprintf(out, size, "-");
        return;
    }
    const int unit = calc_best_unit(qty, base_value);
    char number[24];
    const calc_unit_t *shown = calc_unit(qty, unit);
    (void)calc_format(form_round_significant(base_value / shown->factor, SIGNIFICANT_DIGITS), number, sizeof(number));
    (void)snprintf(out, size, "%s %s", number, shown->symbol);
}

ui_rect_t form_button_rect(int top, int height, int count, int index)
{
    const int width = SCREEN_WIDTH / count;
    return (ui_rect_t){(int16_t)(index * width), (int16_t)top, (int16_t)width, (int16_t)height};
}
