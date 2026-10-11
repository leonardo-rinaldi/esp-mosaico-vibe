// SPDX-License-Identifier: Apache-2.0
#include "screen_conv.h"
#include <math.h>
#include "i18n.h"
#include "keypad.h"
#include "ui_font.h"

enum {
    QTY_TOP = 48, QTY_HEIGHT = 38, QTY_WIDTH = 96,
    DISPLAY_TOP = 92, DISPLAY_HEIGHT = 56, DISPLAY_MARGIN = 6,
    UNIT_TOP = 152, UNIT_HEIGHT = 38, UNIT_WIDTH = 120,
    ROWS_TOP = 196, ROW_HEIGHT = 30, ROW_FILL = 28,
    ENTRY_RIGHT = 458, CURSOR_WIDTH = 4,
    ENTRY_SCALE = 5, VALUE_SCALE = 3, LABEL_SCALE = 2,
};

static const char *const s_qty_label[CALC_QTY_COUNT] = {"Ω", "V", "A", "F", "W"};

static void refresh(conv_state_t *state)
{
    double value = 0.0;
    const bool has_value = calc_entry_value(&state->entry, &value);
    for (int i = 0; i < CALC_UNITS_PER_QTY; ++i) {
        const double converted = has_value ? calc_convert(state->qty, value, state->unit, i) : NAN;
        char text[UI_LINE_MAX];
        (void)calc_format(converted, text, sizeof(text));
        ui_lines_printf(&state->view, i, "%s", text);
    }
    char entry[UI_LINE_MAX];
    (void)calc_entry_display(&state->entry, entry, sizeof(entry));
    ui_lines_printf(&state->view, CONV_LINE_ENTRY, "%s", entry);
    state->best = has_value
        ? calc_best_unit(state->qty, value * calc_unit(state->qty, state->unit)->factor)
        : -1;
}

void conv_init(conv_state_t *state)
{
    state->qty = CALC_QTY_RESISTANCE;
    state->unit = calc_default_unit(state->qty);
    calc_entry_clear(&state->entry);
    ui_lines_init(&state->view, CONV_LINE_ENTRY + 1);
    refresh(state);
}

void conv_tap(conv_state_t *state, int32_t x, int32_t y)
{
    if (y >= QTY_TOP && y < QTY_TOP + QTY_HEIGHT) {
        const int qty = (int)(x / QTY_WIDTH);
        if (qty < CALC_QTY_COUNT && (calc_qty_t)qty != state->qty) {
            state->qty = (calc_qty_t)qty;
            state->unit = calc_default_unit(state->qty);
        }
    } else if (y >= UNIT_TOP && y < UNIT_TOP + UNIT_HEIGHT) {
        const int unit = (int)(x / UNIT_WIDTH);
        if (unit < CALC_UNITS_PER_QTY) state->unit = unit;
    } else if (!keypad_apply(&state->entry, x, y)) {
        return;
    }
    refresh(state);
}

void conv_draw(const conv_state_t *state, const ui_pointer_t *pointer, uint32_t tick)
{
    for (int i = 0; i < CALC_QTY_COUNT; ++i) {
        const ui_rect_t rect = {(int16_t)(i * QTY_WIDTH), QTY_TOP, QTY_WIDTH, QTY_HEIGHT};
        ui_button(rect, s_qty_label[i], 3,
                  (int)state->qty == i ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    }

    ui_fill((ui_rect_t){DISPLAY_MARGIN, DISPLAY_TOP, 480 - 2 * DISPLAY_MARGIN, DISPLAY_HEIGHT},
            UI_COLOR_PANEL);
    ui_text_draw(i18n_text(TXT_CONV_VALUE), 14, DISPLAY_TOP + 8, LABEL_SCALE, UI_COLOR_MUTED);
    const char *entry = ui_lines_get(&state->view, CONV_LINE_ENTRY);
    const bool empty = !entry || !*entry;
    ui_text_draw_right(empty ? "0" : entry, ENTRY_RIGHT,
                       DISPLAY_TOP + (DISPLAY_HEIGHT - ui_text_height(ENTRY_SCALE)) / 2, ENTRY_SCALE,
                       empty ? UI_COLOR_MUTED : UI_COLOR_TEXT);
    if ((tick / UI_BLINK_TICKS) % 2 == 0)
        ui_fill((ui_rect_t){ENTRY_RIGHT + 6, DISPLAY_TOP + 8, CURSOR_WIDTH, DISPLAY_HEIGHT - 16},
                UI_COLOR_ACCENT);

    for (int i = 0; i < CALC_UNITS_PER_QTY; ++i) {
        const ui_rect_t rect = {(int16_t)(i * UNIT_WIDTH), UNIT_TOP, UNIT_WIDTH, UNIT_HEIGHT};
        ui_button(rect, calc_unit(state->qty, i)->symbol, 3,
                  state->unit == i ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    }

    for (int i = 0; i < CALC_UNITS_PER_QTY; ++i) {
        const int top = ROWS_TOP + i * ROW_HEIGHT;
        const bool best = state->best == i;
        ui_fill((ui_rect_t){DISPLAY_MARGIN, (int16_t)top, 480 - 2 * DISPLAY_MARGIN, ROW_FILL},
                best ? UI_COLOR_OK_BG : UI_COLOR_PANEL);
        const Color color = best ? UI_COLOR_OK : UI_COLOR_TEXT;
        ui_text_draw(calc_unit(state->qty, i)->symbol, 16, top + (ROW_FILL - ui_text_height(LABEL_SCALE)) / 2,
                     LABEL_SCALE, best ? UI_COLOR_OK : UI_COLOR_MUTED);
        ui_text_draw_right(ui_lines_get(&state->view, i), 466,
                           top + (ROW_FILL - ui_text_height(VALUE_SCALE)) / 2, VALUE_SCALE, color);
    }
    keypad_draw(pointer);
}
