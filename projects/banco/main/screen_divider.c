// SPDX-License-Identifier: Apache-2.0
#include "screen_divider.h"
#include <math.h>
#include <stdio.h>
#include "calc_bench.h"
#include "form.h"
#include "i18n.h"
#include "keypad.h"
#include "ui_font.h"

enum {
    BUTTONS_TOP = 48, BUTTON_HEIGHT = 36, FIELDS_TOP = 90, RESULT_TOP = 226, RESULT_HEIGHT = 88,
    BIG_SCALE = 4, ROW_SCALE = 2, DEFAULT_UNIT = 1,
};

static const double s_unit_factor[DIVIDER_UNITS] = {1.0, 1e3, 1e6};
static const char *const s_unit_symbol[DIVIDER_UNITS] = {"Ω", "kΩ", "MΩ"};
/* Fixed labels: they read the same in both languages. */
static const char *const s_label[DIVIDER_MODE_COUNT][DIVIDER_FIELDS] = {
    {"VIN", "R1", "R2"}, {"VIN", "VOUT", "R1"},
};
static const char *const s_field_unit[DIVIDER_MODE_COUNT][DIVIDER_FIELDS] = {{"V", NULL, NULL}, {"V", "V", NULL}};

static const char *field_unit(const divider_state_t *state, int field)
{
    const char *fixed = s_field_unit[state->mode][field];
    return fixed ? fixed : s_unit_symbol[state->unit];
}

static void refresh(divider_state_t *state)
{
    const calc_entry_t *entry = state->entry[state->mode];
    double value[DIVIDER_FIELDS] = {0};
    bool complete = true;
    for (int i = 0; i < DIVIDER_FIELDS; ++i) {
        const bool has = calc_entry_value(&entry[i], &value[i]);
        complete &= has && value[i] > 0.0;
        char text[UI_LINE_MAX];
        (void)calc_entry_display(&entry[i], text, sizeof(text));
        ui_lines_printf(&state->view, DIVIDER_LINE_ENTRIES + i, "%s", text);
    }
    ui_lines_printf(&state->view, DIVIDER_LINE_RESULT, "%s", "");
    ui_lines_printf(&state->view, DIVIDER_LINE_EXTRA, "%s", "");
    ui_lines_printf(&state->view, DIVIDER_LINE_HINT, "%s", "");
    state->valid = false;
    if (!complete) {
        ui_lines_printf(&state->view, DIVIDER_LINE_HINT, "%s", i18n_text(TXT_ENTER_THREE));
        return;
    }
    const double scale = s_unit_factor[state->unit];
    char text[UI_LINE_MAX], extra[UI_LINE_MAX];
    if (state->mode == DIVIDER_FIND_VOUT) {
        const double vin = value[0], r1 = value[1] * scale, r2 = value[2] * scale;
        form_format_quantity(CALC_QTY_VOLTAGE, divider_vout(vin, r1, r2), text, sizeof(text));
        form_format_quantity(CALC_QTY_CURRENT, vin / (r1 + r2), extra, sizeof(extra));
        ui_lines_printf(&state->view, DIVIDER_LINE_EXTRA, "%s %s", i18n_text(TXT_DIV_CURRENT), extra);
    } else {
        const double vin = value[0], vout = value[1], r1 = value[2] * scale;
        double r2;
        if (!divider_r2(vin, vout, r1, &r2)) {
            ui_lines_printf(&state->view, DIVIDER_LINE_HINT, "%s", i18n_text(TXT_DIV_RANGE));
            return;
        }
        form_format_quantity(CALC_QTY_RESISTANCE, r2, text, sizeof(text));
        const double nearest = e24_nearest(r2);
        form_format_quantity(CALC_QTY_RESISTANCE, nearest, extra, sizeof(extra));
        char out[UI_LINE_MAX];
        form_format_quantity(CALC_QTY_VOLTAGE, divider_vout(vin, r1, nearest), out, sizeof(out));
        ui_lines_printf(&state->view, DIVIDER_LINE_EXTRA, "E24 %s = %s", extra, out);
    }
    ui_lines_printf(&state->view, DIVIDER_LINE_RESULT, "%s", text);
    state->valid = true;
}

void divider_relocalize(divider_state_t *state)
{
    refresh(state);
}

void divider_init(divider_state_t *state)
{
    ui_lines_init(&state->view, DIVIDER_LINE_COUNT);
    for (int m = 0; m < DIVIDER_MODE_COUNT; ++m)
        for (int i = 0; i < DIVIDER_FIELDS; ++i) calc_entry_clear(&state->entry[m][i]);
    state->mode = DIVIDER_FIND_VOUT;
    state->unit = DEFAULT_UNIT;
    state->selected = 0;
    refresh(state);
}

static ui_rect_t button(int index)
{
    return form_button_rect(BUTTONS_TOP, BUTTON_HEIGHT, DIVIDER_MODE_COUNT + 1, index);
}

void divider_tap(divider_state_t *state, int32_t x, int32_t y)
{
    for (int m = 0; m < DIVIDER_MODE_COUNT; ++m) {
        if (ui_rect_contains(button(m), x, y)) {
            state->mode = (divider_mode_t)m;
            state->selected = 0;
            refresh(state);
            return;
        }
    }
    if (ui_rect_contains(button(DIVIDER_MODE_COUNT), x, y)) {
        state->unit = (state->unit + 1) % DIVIDER_UNITS;
        refresh(state);
        return;
    }
    const int field = form_field_hit(FIELDS_TOP, DIVIDER_FIELDS, x, y);
    if (field >= 0) {
        state->selected = field;
        return;
    }
    if (keypad_apply(&state->entry[state->mode][state->selected], x, y)) refresh(state);
}

void divider_draw(const divider_state_t *state, const ui_pointer_t *pointer)
{
    ui_button(button(DIVIDER_FIND_VOUT), i18n_text(TXT_DIV_FIND_VOUT), ROW_SCALE,
              state->mode == DIVIDER_FIND_VOUT ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(button(DIVIDER_FIND_R2), i18n_text(TXT_DIV_FIND_R2), ROW_SCALE,
              state->mode == DIVIDER_FIND_R2 ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(button(DIVIDER_MODE_COUNT), s_unit_symbol[state->unit], ROW_SCALE, UI_BUTTON_FUNCTION, pointer);
    for (int i = 0; i < DIVIDER_FIELDS; ++i)
        form_draw_field(form_field_rect(FIELDS_TOP, i), s_label[state->mode][i],
                        ui_lines_get(&state->view, DIVIDER_LINE_ENTRIES + i), field_unit(state, i),
                        state->selected == i);
    const ui_rect_t box = {6, RESULT_TOP, 468, RESULT_HEIGHT};
    ui_fill(box, state->valid ? UI_COLOR_OK_BG : UI_COLOR_PANEL);
    if (state->valid) {
        ui_text_draw(state->mode == DIVIDER_FIND_VOUT ? "VOUT" : "R2", 16, box.y + 14, ROW_SCALE, UI_COLOR_MUTED);
        ui_text_draw_right(ui_lines_get(&state->view, DIVIDER_LINE_RESULT), 464, box.y + 8, BIG_SCALE,
                           UI_COLOR_OK);
        ui_text_draw_right(ui_lines_get(&state->view, DIVIDER_LINE_EXTRA), 464, box.y + 56, ROW_SCALE,
                           UI_COLOR_TEXT);
    } else {
        ui_text_draw(ui_lines_get(&state->view, DIVIDER_LINE_HINT), 16, box.y + 14, ROW_SCALE, UI_COLOR_MUTED);
    }
    keypad_draw(pointer);
}
