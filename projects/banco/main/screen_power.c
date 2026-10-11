// SPDX-License-Identifier: Apache-2.0
#include "screen_power.h"
#include "form.h"
#include "i18n.h"
#include "ui_font.h"

enum {
    BRIGHTNESS_LABEL_Y = 66, BRIGHTNESS_TOP = 94, SLEEP_LABEL_Y = 190, SLEEP_TOP = 218, BUTTON_HEIGHT = 60,
    NOTE_Y = 318, NOTE_PITCH = 28, LABEL_SCALE = 2, BUTTON_SCALE = 2,
};

static const int s_brightness_percent[POWER_BRIGHTNESS_COUNT] = {100, 60, 30};
static const int s_sleep_seconds[POWER_SLEEP_COUNT] = {0, 30, 60, 120};
static const text_id_t s_brightness_name[POWER_BRIGHTNESS_COUNT] = {TXT_POWER_HIGH, TXT_POWER_MEDIUM, TXT_POWER_LOW};
static const char *const s_sleep_label[POWER_SLEEP_COUNT] = {NULL, "30 S", "1 MIN", "2 MIN"};

static const char *sleep_label(int index)
{
    return index == 0 ? i18n_text(TXT_POWER_NEVER) : s_sleep_label[index];
}

void power_relocalize(power_state_t *state)
{
    ui_lines_printf(&state->view, POWER_LINE_BRIGHTNESS, "%s", i18n_text(s_brightness_name[state->brightness]));
    ui_lines_printf(&state->view, POWER_LINE_SLEEP, "%s", sleep_label(state->sleep));
}

void power_init(power_state_t *state)
{
    state->brightness = 0;
    state->sleep = 2;                   /* Off after a minute: the usual bench use. */
    ui_lines_init(&state->view, POWER_LINE_COUNT);
    power_relocalize(state);
}

int power_brightness_percent(const power_state_t *state)
{
    return s_brightness_percent[state->brightness < POWER_BRIGHTNESS_COUNT ? state->brightness : 0];
}

int power_sleep_seconds(const power_state_t *state)
{
    return s_sleep_seconds[state->sleep < POWER_SLEEP_COUNT ? state->sleep : 0];
}

static ui_rect_t brightness_rect(int index)
{
    return form_button_rect(BRIGHTNESS_TOP, BUTTON_HEIGHT, POWER_BRIGHTNESS_COUNT, index);
}

static ui_rect_t sleep_rect(int index)
{
    return form_button_rect(SLEEP_TOP, BUTTON_HEIGHT, POWER_SLEEP_COUNT, index);
}

void power_tap(power_state_t *state, int32_t x, int32_t y)
{
    for (int i = 0; i < POWER_BRIGHTNESS_COUNT; ++i) {
        if (ui_rect_contains(brightness_rect(i), x, y)) {
            state->brightness = (uint8_t)i;
            power_relocalize(state);
            return;
        }
    }
    for (int i = 0; i < POWER_SLEEP_COUNT; ++i) {
        if (ui_rect_contains(sleep_rect(i), x, y)) {
            state->sleep = (uint8_t)i;
            power_relocalize(state);
            return;
        }
    }
}

void power_draw(const power_state_t *state, const ui_pointer_t *pointer)
{
    ui_text_draw(i18n_text(TXT_POWER_BRIGHTNESS), 12, BRIGHTNESS_LABEL_Y, LABEL_SCALE, UI_COLOR_MUTED);
    for (int i = 0; i < POWER_BRIGHTNESS_COUNT; ++i)
        ui_button(brightness_rect(i), i18n_text(s_brightness_name[i]), BUTTON_SCALE,
                  state->brightness == i ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_text_draw(i18n_text(TXT_POWER_SLEEP), 12, SLEEP_LABEL_Y, LABEL_SCALE, UI_COLOR_MUTED);
    for (int i = 0; i < POWER_SLEEP_COUNT; ++i)
        ui_button(sleep_rect(i), sleep_label(i), BUTTON_SCALE,
                  state->sleep == i ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_text_draw(i18n_text(TXT_POWER_NOTE1), 12, NOTE_Y, LABEL_SCALE, UI_COLOR_MUTED);
    ui_text_draw(i18n_text(TXT_POWER_NOTE2), 12, NOTE_Y + NOTE_PITCH, LABEL_SCALE, UI_COLOR_MUTED);
}
