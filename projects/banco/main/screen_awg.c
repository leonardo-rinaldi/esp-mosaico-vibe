// SPDX-License-Identifier: Apache-2.0
#include "screen_awg.h"
#include <math.h>
#include <stdio.h>
#include "calc_bench.h"
#include "form.h"
#include "i18n.h"
#include "keypad.h"
#include "ui_font.h"

enum {
    MODES_TOP = 48, MODES_HEIGHT = 40, FIELD_TOP = 92, NOTE_Y = 138,
    RESULT_TOP = 164, RESULT_HEIGHT = 150, ROW_PITCH = 46, ROW_SCALE = 2, VALUE_SCALE = 3,
    SIGNIFICANT_DIGITS = 4,
};

static void format_number(double value, const char *unit, char *out, size_t size)
{
    char number[24];
    (void)calc_format(form_round_significant(value, SIGNIFICANT_DIGITS), number, sizeof(number));
    (void)snprintf(out, size, "%s %s", number, unit);
}

/* AWG numbers below zero are written 00, 000, 0000. */
static void awg_name(int awg, char *out, size_t size)
{
    if (awg >= 0) (void)snprintf(out, size, "%d", awg);
    else (void)snprintf(out, size, "%s", awg == -1 ? "00" : (awg == -2 ? "000" : "0000"));
}

static void show_awg_rows(awg_state_t *state, int awg, bool first_row_is_area)
{
    char text[UI_LINE_MAX];
    format_number(awg_area_mm2(awg), "mm²", text, sizeof(text));
    ui_lines_printf(&state->view, first_row_is_area ? AWG_LINE_ROW1 : AWG_LINE_ROW2, "%s", text);
    format_number(awg_diameter_mm(awg), "mm", text, sizeof(text));
    ui_lines_printf(&state->view, first_row_is_area ? AWG_LINE_ROW2 : AWG_LINE_ROW3, "%s", text);
}

static void refresh(awg_state_t *state)
{
    const calc_entry_t *entry = &state->entry[state->mode];
    char text[UI_LINE_MAX];
    (void)calc_entry_display(entry, text, sizeof(text));
    ui_lines_printf(&state->view, AWG_LINE_ENTRY, "%s", text);
    for (int i = AWG_LINE_ROW1; i <= AWG_LINE_ALIAS; ++i) ui_lines_printf(&state->view, i, "%s", "");
    state->valid = false;

    double value = 0.0;
    if (!calc_entry_value(entry, &value)) {
        ui_lines_printf(&state->view, AWG_LINE_HINT, "%s", i18n_text(TXT_AWG_SIGN));
        return;
    }
    if (state->mode == AWG_MODE_TO_MM2) {
        if (fabs(value - round(value)) > 1e-9 || value < AWG_MIN || value > AWG_MAX) {
            ui_lines_printf(&state->view, AWG_LINE_HINT, "%s", i18n_text(TXT_AWG_BAD));
            return;
        }
        const int awg = (int)lround(value);
        show_awg_rows(state, awg, true);
        const double metric = metric_section_at_least(awg_area_mm2(awg));
        if (metric > 0.0) {
            format_number(metric, "mm²", text, sizeof(text));
            ui_lines_printf(&state->view, AWG_LINE_ROW3, "%s", text);
        } else {
            ui_lines_printf(&state->view, AWG_LINE_ROW3, "-");
        }
        if (awg < 0) {
            awg_name(awg, text, sizeof(text));
            ui_lines_printf(&state->view, AWG_LINE_ALIAS, "= %s", text);
        }
    } else {
        if (!(value > 0.0)) {
            ui_lines_printf(&state->view, AWG_LINE_HINT, "%s", i18n_text(TXT_AWG_SIGN));
            return;
        }
        const int awg = awg_nearest(value);
        awg_name(awg, text, sizeof(text));
        ui_lines_printf(&state->view, AWG_LINE_ROW1, "%s", text);
        show_awg_rows(state, awg, false);
    }
    state->valid = true;
}

void awg_relocalize(awg_state_t *state)
{
    refresh(state);
}

void awg_init(awg_state_t *state)
{
    ui_lines_init(&state->view, AWG_LINE_COUNT);
    for (int m = 0; m < AWG_MODE_COUNT; ++m) calc_entry_clear(&state->entry[m]);
    state->mode = AWG_MODE_TO_MM2;
    refresh(state);
}

static ui_rect_t mode_rect(int index)
{
    return form_button_rect(MODES_TOP, MODES_HEIGHT, AWG_MODE_COUNT, index);
}

void awg_tap(awg_state_t *state, int32_t x, int32_t y)
{
    for (int m = 0; m < AWG_MODE_COUNT; ++m) {
        if (ui_rect_contains(mode_rect(m), x, y)) {
            state->mode = (awg_mode_t)m;
            refresh(state);
            return;
        }
    }
    if (keypad_apply(&state->entry[state->mode], x, y)) refresh(state);
}

static void draw_row(int y, const char *label, const char *value)
{
    ui_text_draw(label, 16, y + 6, ROW_SCALE, UI_COLOR_MUTED);
    ui_text_draw_right(value && *value ? value : "-", 464, y, VALUE_SCALE, value && *value ? UI_COLOR_OK : UI_COLOR_MUTED);
}

void awg_draw(const awg_state_t *state, const ui_pointer_t *pointer)
{
    const bool to_mm2 = state->mode == AWG_MODE_TO_MM2;
    ui_button(mode_rect(AWG_MODE_TO_MM2), i18n_text(TXT_AWG_TO_MM), ROW_SCALE,
              to_mm2 ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(mode_rect(AWG_MODE_TO_AWG), i18n_text(TXT_AWG_TO_AWG), ROW_SCALE,
              to_mm2 ? UI_BUTTON_NORMAL : UI_BUTTON_SELECTED, pointer);
    const ui_rect_t field = form_field_rect(FIELD_TOP, 0);
    const char *alias = ui_lines_get(&state->view, AWG_LINE_ALIAS);
    form_draw_field(field, to_mm2 ? "AWG" : "mm²", ui_lines_get(&state->view, AWG_LINE_ENTRY),
                    to_mm2 ? alias : "mm²", true);
    if (to_mm2) ui_text_draw_center(i18n_text(TXT_AWG_SIGN), 240, NOTE_Y, 1, UI_COLOR_MUTED);

    const ui_rect_t box = {6, RESULT_TOP, 468, RESULT_HEIGHT};
    ui_fill(box, state->valid ? UI_COLOR_OK_BG : UI_COLOR_PANEL);
    if (state->valid) {
        draw_row(box.y + 10, i18n_text(to_mm2 ? TXT_AWG_AREA : TXT_AWG_NEAREST), ui_lines_get(&state->view, AWG_LINE_ROW1));
        draw_row(box.y + 10 + ROW_PITCH, i18n_text(to_mm2 ? TXT_AWG_DIAMETER : TXT_AWG_AREA),
                 ui_lines_get(&state->view, AWG_LINE_ROW2));
        draw_row(box.y + 10 + 2 * ROW_PITCH, i18n_text(to_mm2 ? TXT_AWG_METRIC : TXT_AWG_DIAMETER),
                 ui_lines_get(&state->view, AWG_LINE_ROW3));
    } else {
        const char *hint = ui_lines_get(&state->view, AWG_LINE_HINT);
        ui_text_draw(hint, 16, box.y + 14, ROW_SCALE, UI_COLOR_MUTED);
    }
    keypad_draw(pointer);
}
