// SPDX-License-Identifier: Apache-2.0
#include "screen_led.h"
#include <stdio.h>
#include "calc_bench.h"
#include "form.h"
#include "i18n.h"
#include "keypad.h"
#include "ui_font.h"

enum {
    FIELDS_TOP = 48, RESULT_TOP = 184, RESULT_HEIGHT = 130,
    DEFAULT_LED_VOLTS = 2, DEFAULT_CURRENT_MA = 20,
    BIG_SCALE = 4, ROW_SCALE = 2, MILLI = 1000,
};

static const text_id_t s_label[LED_FIELD_COUNT] = {TXT_LED_SUPPLY, TXT_LED_VOLTAGE, TXT_LED_CURRENT};
static const char *const s_unit[LED_FIELD_COUNT] = {"V", "V", "mA"};

static void clear_results(led_state_t *state)
{
    for (int i = LED_LINE_STANDARD; i <= LED_LINE_RATING; ++i) ui_lines_printf(&state->view, i, "%s", "");
}

static void refresh(led_state_t *state)
{
    double value[LED_FIELD_COUNT] = {0};
    bool complete = true;
    for (int i = 0; i < LED_FIELD_COUNT; ++i) {
        complete &= calc_entry_value(&state->entry[i], &value[i]) && value[i] > 0.0;
        char text[UI_LINE_MAX];
        (void)calc_entry_display(&state->entry[i], text, sizeof(text));
        ui_lines_printf(&state->view, LED_LINE_ENTRIES + i, "%s", text);
    }
    clear_results(state);
    ui_lines_printf(&state->view, LED_LINE_HINT, "%s", "");
    state->valid = false;
    if (!complete) {
        ui_lines_printf(&state->view, LED_LINE_HINT, "%s", i18n_text(TXT_ENTER_THREE));
        return;
    }
    const led_input_t input = {value[LED_FIELD_SUPPLY], value[LED_FIELD_LED], value[LED_FIELD_CURRENT] / MILLI};
    led_result_t result;
    if (!calc_led_resistor(&input, &result)) {
        ui_lines_printf(&state->view, LED_LINE_HINT, "%s", i18n_text(TXT_LED_LOW));
        return;
    }
    char text[UI_LINE_MAX];
    form_format_quantity(CALC_QTY_RESISTANCE, result.std_ohm, text, sizeof(text));
    ui_lines_printf(&state->view, LED_LINE_STANDARD, "%s", text);
    form_format_quantity(CALC_QTY_RESISTANCE, result.ideal_ohm, text, sizeof(text));
    ui_lines_printf(&state->view, LED_LINE_EXACT, "%s", text);
    form_format_quantity(CALC_QTY_CURRENT, result.actual_a, text, sizeof(text));
    ui_lines_printf(&state->view, LED_LINE_ACTUAL, "%s", text);
    form_format_quantity(CALC_QTY_POWER, result.power_w, text, sizeof(text));
    ui_lines_printf(&state->view, LED_LINE_POWER, "%s", text);
    ui_lines_printf(&state->view, LED_LINE_RATING, "%s", power_rating_label(result.power_w));
    state->valid = true;
}

void led_relocalize(led_state_t *state)
{
    refresh(state);
}

void led_init(led_state_t *state)
{
    ui_lines_init(&state->view, LED_LINE_COUNT);
    for (int i = 0; i < LED_FIELD_COUNT; ++i) calc_entry_clear(&state->entry[i]);
    (void)calc_entry_digit(&state->entry[LED_FIELD_LED], (char)('0' + DEFAULT_LED_VOLTS));
    (void)calc_entry_digit(&state->entry[LED_FIELD_CURRENT], (char)('0' + DEFAULT_CURRENT_MA / 10));
    (void)calc_entry_digit(&state->entry[LED_FIELD_CURRENT], '0');
    state->selected = LED_FIELD_SUPPLY;
    refresh(state);
}

void led_tap(led_state_t *state, int32_t x, int32_t y)
{
    const int field = form_field_hit(FIELDS_TOP, LED_FIELD_COUNT, x, y);
    if (field >= 0) {
        state->selected = field;
        return;
    }
    if (keypad_apply(&state->entry[state->selected], x, y)) refresh(state);
}

static void draw_row(int y, text_id_t label, const char *value, Color color)
{
    ui_text_draw(i18n_text(label), 16, y, ROW_SCALE, UI_COLOR_MUTED);
    ui_text_draw_right(value, 464, y, ROW_SCALE, color);
}

void led_draw(const led_state_t *state, const ui_pointer_t *pointer)
{
    for (int i = 0; i < LED_FIELD_COUNT; ++i)
        form_draw_field(form_field_rect(FIELDS_TOP, i), i18n_text(s_label[i]),
                        ui_lines_get(&state->view, LED_LINE_ENTRIES + i), s_unit[i], state->selected == i);
    const ui_rect_t box = {6, RESULT_TOP, 468, RESULT_HEIGHT};
    ui_fill(box, state->valid ? UI_COLOR_OK_BG : UI_COLOR_PANEL);
    if (state->valid) {
        ui_text_draw(i18n_text(TXT_LED_RESISTOR), 16, box.y + 14, ROW_SCALE, UI_COLOR_MUTED);
        ui_text_draw_right(ui_lines_get(&state->view, LED_LINE_STANDARD), 464, box.y + 8, BIG_SCALE, UI_COLOR_OK);
        draw_row(box.y + 46, TXT_LED_IDEAL, ui_lines_get(&state->view, LED_LINE_EXACT), UI_COLOR_TEXT);
        draw_row(box.y + 70, TXT_LED_ACTUAL, ui_lines_get(&state->view, LED_LINE_ACTUAL), UI_COLOR_TEXT);
        char power[UI_LINE_MAX];
        (void)snprintf(power, sizeof(power), "%s   %s %s", ui_lines_get(&state->view, LED_LINE_POWER),
                       i18n_text(TXT_LED_USE), ui_lines_get(&state->view, LED_LINE_RATING));
        draw_row(box.y + 94, TXT_LED_POWER, power, UI_COLOR_TEXT);
    } else {
        ui_text_draw(ui_lines_get(&state->view, LED_LINE_HINT), 16, box.y + 14, ROW_SCALE, UI_COLOR_MUTED);
    }
    keypad_draw(pointer);
}
