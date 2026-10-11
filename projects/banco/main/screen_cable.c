// SPDX-License-Identifier: Apache-2.0
#include "screen_cable.h"
#include <math.h>
#include "i18n.h"
#include "keypad.h"
#include "ui_font.h"

enum {
    FIELDS_TOP = 48, FIELD_PITCH = 44, FIELD_HEIGHT = 40,
    LABEL_X = 16, VALUE_RIGHT = 418, UNIT_X = 428, VALUE_SCALE = 3, LABEL_SCALE = 2,
    TOGGLE_MATERIAL_TOP = 184, TOGGLE_SYSTEM_TOP = 224, TOGGLE_HEIGHT = 38, TOGGLE_WIDTH = 240,
    RESULT_TOP = 268, RESULT_HEIGHT = 44,
    DEFAULT_LIMIT_PERCENT = 4,
};

static const text_id_t s_field_label[CABLE_FIELD_COUNT] = {TXT_CABLE_CURRENT, TXT_CABLE_LENGTH,
                                                          TXT_CABLE_MAX_DROP};
static const char *const s_field_unit[CABLE_FIELD_COUNT] = {"A", "m", "%"};

static double round_to_hundredths(double value)
{
    return round(value * 100.0) / 100.0;
}

static void refresh(cable_state_t *state)
{
    double value[CABLE_FIELD_COUNT] = {0};
    bool valid = true;
    for (int i = 0; i < CABLE_FIELD_COUNT; ++i) {
        valid &= calc_entry_value(&state->entry[i], &value[i]) && value[i] > 0.0;
        char text[UI_LINE_MAX];
        (void)calc_entry_display(&state->entry[i], text, sizeof(text));
        ui_lines_printf(&state->view, CABLE_LINE_ENTRIES + i, "%s", text);
    }

    state->no_section = false;
    ui_lines_printf(&state->view, CABLE_LINE_SECTION, "-");
    ui_lines_printf(&state->view, CABLE_LINE_DROP, "%s", "");
    ui_lines_printf(&state->view, CABLE_LINE_HINT, "%s", "");
    if (!valid) {
        ui_lines_printf(&state->view, CABLE_LINE_HINT, "%s", i18n_text(TXT_CABLE_ENTER_VALUES));
        return;
    }

    const cable_input_t input = {
        .current_a = value[CABLE_FIELD_CURRENT], .length_m = value[CABLE_FIELD_LENGTH],
        .limit_percent = value[CABLE_FIELD_LIMIT], .material = state->material,
        .system = state->system,
    };
    double drop_v = 0.0, drop_percent = 0.0;
    const int index = calc_cable_min_section(&input, &drop_v, &drop_percent);
    char number[UI_LINE_MAX], second[UI_LINE_MAX];
    if (index < 0) {
        state->no_section = true;
        (void)calc_format(calc_cable_section(calc_cable_section_count() - 1), number, sizeof(number));
        ui_lines_printf(&state->view, CABLE_LINE_SECTION, "%s %s mm²", i18n_text(TXT_CABLE_OVER), number);
        ui_lines_printf(&state->view, CABLE_LINE_HINT, "%s", i18n_text(TXT_CABLE_NO_SECTION));
        return;
    }
    (void)calc_format(calc_cable_section(index), number, sizeof(number));
    ui_lines_printf(&state->view, CABLE_LINE_SECTION, "%s mm²", number);
    (void)calc_format(round_to_hundredths(drop_v), number, sizeof(number));
    (void)calc_format(round_to_hundredths(drop_percent), second, sizeof(second));
    ui_lines_printf(&state->view, CABLE_LINE_DROP, "%s V   %s %%", number, second);
}

void cable_relocalize(cable_state_t *state)
{
    refresh(state);
}

void cable_init(cable_state_t *state)
{
    ui_lines_init(&state->view, CABLE_LINE_ENTRIES + CABLE_FIELD_COUNT);
    for (int i = 0; i < CABLE_FIELD_COUNT; ++i) calc_entry_clear(&state->entry[i]);
    (void)calc_entry_digit(&state->entry[CABLE_FIELD_LIMIT], (char)('0' + DEFAULT_LIMIT_PERCENT));
    state->selected = CABLE_FIELD_CURRENT;
    state->material = CABLE_COPPER;
    state->system = CABLE_SINGLE_PHASE;
    refresh(state);
}

static ui_rect_t field_rect(int index)
{
    return (ui_rect_t){6, (int16_t)(FIELDS_TOP + index * FIELD_PITCH), 468, FIELD_HEIGHT};
}

static ui_rect_t toggle_rect(int row_top, int index)
{
    return (ui_rect_t){(int16_t)(index * TOGGLE_WIDTH), (int16_t)row_top, TOGGLE_WIDTH, TOGGLE_HEIGHT};
}

void cable_tap(cable_state_t *state, int32_t x, int32_t y)
{
    for (int i = 0; i < CABLE_FIELD_COUNT; ++i) {
        if (ui_rect_contains(field_rect(i), x, y)) {
            state->selected = i;
            return;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (ui_rect_contains(toggle_rect(TOGGLE_MATERIAL_TOP, i), x, y)) {
            state->material = i == 0 ? CABLE_COPPER : CABLE_ALUMINIUM;
            refresh(state);
            return;
        }
        if (ui_rect_contains(toggle_rect(TOGGLE_SYSTEM_TOP, i), x, y)) {
            state->system = i == 0 ? CABLE_SINGLE_PHASE : CABLE_THREE_PHASE;
            refresh(state);
            return;
        }
    }
    if (keypad_apply(&state->entry[state->selected], x, y)) refresh(state);
}

void cable_draw(const cable_state_t *state, const ui_pointer_t *pointer)
{
    for (int i = 0; i < CABLE_FIELD_COUNT; ++i) {
        const ui_rect_t rect = field_rect(i);
        ui_fill(rect, UI_COLOR_PANEL);
        if (state->selected == i) ui_outline(rect, 3, UI_COLOR_ACCENT);
        const int center = rect.y + rect.h / 2;
        ui_text_draw(i18n_text(s_field_label[i]), LABEL_X, center - ui_text_height(LABEL_SCALE) / 2, LABEL_SCALE,
                     UI_COLOR_MUTED);
        const char *value = ui_lines_get(&state->view, CABLE_LINE_ENTRIES + i);
        const bool empty = !value || !*value;
        ui_text_draw_right(empty ? "-" : value, VALUE_RIGHT, center - ui_text_height(VALUE_SCALE) / 2,
                           VALUE_SCALE, empty ? UI_COLOR_MUTED : UI_COLOR_TEXT);
        ui_text_draw(s_field_unit[i], UNIT_X, center - ui_text_height(LABEL_SCALE) / 2, LABEL_SCALE,
                     UI_COLOR_MUTED);
    }
    ui_button(toggle_rect(TOGGLE_MATERIAL_TOP, 0), i18n_text(TXT_CABLE_COPPER), 2,
              state->material == CABLE_COPPER ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(toggle_rect(TOGGLE_MATERIAL_TOP, 1), i18n_text(TXT_CABLE_ALUMINIUM), 2,
              state->material == CABLE_ALUMINIUM ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(toggle_rect(TOGGLE_SYSTEM_TOP, 0), i18n_text(TXT_CABLE_SINGLE_PHASE), 2,
              state->system == CABLE_SINGLE_PHASE ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(toggle_rect(TOGGLE_SYSTEM_TOP, 1), i18n_text(TXT_CABLE_THREE_PHASE), 2,
              state->system == CABLE_THREE_PHASE ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);

    const ui_rect_t result = {6, RESULT_TOP, 468, RESULT_HEIGHT};
    const char *hint = ui_lines_get(&state->view, CABLE_LINE_HINT);
    const char *drop = ui_lines_get(&state->view, CABLE_LINE_DROP);
    const bool found = drop && *drop;
    ui_fill(result, found ? UI_COLOR_OK_BG : UI_COLOR_PANEL);
    ui_text_draw(i18n_text(TXT_CABLE_MIN_SECTION), 16, result.y + 6, LABEL_SCALE, UI_COLOR_MUTED);
    ui_text_draw_right(ui_lines_get(&state->view, CABLE_LINE_SECTION), 464, result.y + 4, VALUE_SCALE,
                       found ? UI_COLOR_OK : UI_COLOR_TEXT);
    if (found)
        ui_text_draw_right(drop, 464, result.y + 27, LABEL_SCALE, UI_COLOR_OK);
    else if (hint && *hint)
        ui_text_draw(hint, 16, result.y + 27, LABEL_SCALE,
                     state->no_section ? UI_COLOR_BAD : UI_COLOR_MUTED);
    keypad_draw(pointer);
}
