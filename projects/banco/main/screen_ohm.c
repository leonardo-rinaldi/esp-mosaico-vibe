// SPDX-License-Identifier: Apache-2.0
#include "screen_ohm.h"
#include <math.h>
#include "keypad.h"
#include "ui_font.h"

enum {
    ROWS_TOP = 50, ROW_PITCH = 56, ROW_HEIGHT = 52,
    UNIT_LEFT = 386, UNIT_WIDTH = 84, UNIT_INSET = 6, UNIT_HEIGHT = 40,
    LABEL_X = 22, TAG_X = 80, VALUE_RIGHT = 372,
    LABEL_SCALE = 4, TAG_SCALE = 2, VALUE_SCALE = 4,
    HINT_Y = 284,
};

static const calc_qty_t s_qty[OHM_COUNT] = {
    [OHM_V] = CALC_QTY_VOLTAGE, [OHM_I] = CALC_QTY_CURRENT,
    [OHM_R] = CALC_QTY_RESISTANCE, [OHM_P] = CALC_QTY_POWER,
};
static const char *const s_name[OHM_COUNT] = {"V", "I", "R", "P"};

static void refresh(ohm_state_t *state)
{
    double in[OHM_COUNT] = {0};
    double out[OHM_COUNT];
    state->known_count = 0;
    for (int i = 0; i < OHM_COUNT; ++i) {
        double value = 0.0;
        state->known[i] = calc_entry_value(&state->entry[i], &value);
        if (state->known[i]) {
            in[i] = value * calc_unit(s_qty[i], state->unit[i])->factor;
            ++state->known_count;
        }
    }
    state->status = calc_ohm(state->known, in, out);

    for (int i = 0; i < OHM_COUNT; ++i) {
        state->computed[i] = state->status == OHM_OK && !state->known[i];
        int shown_unit = state->unit[i];
        char text[UI_LINE_MAX] = "";
        if (state->known[i]) {
            (void)calc_entry_display(&state->entry[i], text, sizeof(text));
        } else if (state->computed[i]) {
            if (state->unit_auto[i]) shown_unit = calc_best_unit(s_qty[i], out[i]);
            (void)calc_format(out[i] / calc_unit(s_qty[i], shown_unit)->factor, text, sizeof(text));
        }
        ui_lines_printf(&state->view, i, "%s", text);
        ui_lines_printf(&state->view, OHM_LINE_UNITS + i, "%s", calc_unit(s_qty[i], shown_unit)->symbol);
    }

    const char *hint = "";
    if (state->status == OHM_NEED_TWO)
        hint = state->known_count < 2 ? "INSERISCI DUE VALORI" : "TROPPI VALORI: CANCELLA UNA RIGA";
    else if (state->status == OHM_UNDEFINED)
        hint = "CALCOLO IMPOSSIBILE";
    ui_lines_printf(&state->view, OHM_LINE_HINT, "%s", hint);
}

void ohm_init(ohm_state_t *state)
{
    ui_lines_init(&state->view, OHM_LINE_UNITS + OHM_COUNT);
    state->selected = OHM_V;
    for (int i = 0; i < OHM_COUNT; ++i) {
        calc_entry_clear(&state->entry[i]);
        state->unit[i] = calc_default_unit(s_qty[i]);
        state->unit_auto[i] = true;
    }
    refresh(state);
}

static ui_rect_t row_rect(int row)
{
    return (ui_rect_t){6, (int16_t)(ROWS_TOP + row * ROW_PITCH), 468, ROW_HEIGHT};
}

static ui_rect_t unit_rect(int row)
{
    return (ui_rect_t){UNIT_LEFT, (int16_t)(ROWS_TOP + row * ROW_PITCH + UNIT_INSET), UNIT_WIDTH,
                       UNIT_HEIGHT};
}

void ohm_tap(ohm_state_t *state, int32_t x, int32_t y)
{
    for (int i = 0; i < OHM_COUNT; ++i) {
        if (ui_rect_contains(unit_rect(i), x, y)) {
            state->selected = i;
            state->unit[i] = (state->unit[i] + 1) % CALC_UNITS_PER_QTY;
            state->unit_auto[i] = false;
            refresh(state);
            return;
        }
        if (ui_rect_contains(row_rect(i), x, y)) {
            state->selected = i;
            return;
        }
    }
    if (keypad_apply(&state->entry[state->selected], x, y)) refresh(state);
}

void ohm_draw(const ohm_state_t *state, const ui_pointer_t *pointer)
{
    for (int i = 0; i < OHM_COUNT; ++i) {
        const ui_rect_t row = row_rect(i);
        ui_fill(row, UI_COLOR_PANEL);
        if (state->selected == i) ui_outline(row, 3, UI_COLOR_ACCENT);
        const int center = row.y + row.h / 2;
        ui_text_draw(s_name[i], LABEL_X, center - ui_text_height(LABEL_SCALE) / 2, LABEL_SCALE,
                     UI_COLOR_ACCENT);
        const char *tag = state->known[i] ? "IN" : (state->computed[i] ? "CALC" : "");
        ui_text_draw(tag, TAG_X, center - ui_text_height(TAG_SCALE) / 2, TAG_SCALE,
                     state->computed[i] ? UI_COLOR_OK : UI_COLOR_MUTED);
        const char *value = ui_lines_get(&state->view, i);
        ui_text_draw_right(value, VALUE_RIGHT, center - ui_text_height(VALUE_SCALE) / 2, VALUE_SCALE,
                           state->computed[i] ? UI_COLOR_OK : UI_COLOR_TEXT);
        ui_button(unit_rect(i), ui_lines_get(&state->view, OHM_LINE_UNITS + i), 3, UI_BUTTON_NORMAL,
                  pointer);
    }
    const char *hint = ui_lines_get(&state->view, OHM_LINE_HINT);
    const bool error = state->status == OHM_UNDEFINED || state->known_count > 2;
    ui_text_draw_center(hint, 240, HINT_Y, 2, error ? UI_COLOR_BAD : UI_COLOR_MUTED);
    keypad_draw(pointer);
}
