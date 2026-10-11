// SPDX-License-Identifier: Apache-2.0
#include "screen_battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i18n.h"
#include "ui_font.h"

enum {
    ICON_X = 90, ICON_Y = 64, ICON_W = 300, ICON_H = 120,
    PERCENT_Y = 204, PERCENT_SCALE = 7, STATE_Y = 268, STATE_SCALE = 3,
    ROWS_TOP = 316, ROW_PITCH = 34, ROW_LABEL_X = 24, ROW_VALUE_RIGHT = 456, ROW_SCALE = 2,
    WARNING_Y = 452,
    ICON_BORDER = 4, ICON_NUB_W = 12, ICON_INSET = 6,
};

#define PERCENT_MAX 100

static int clamp_percent(int percent)
{
    return percent < 0 ? 0 : (percent > PERCENT_MAX ? PERCENT_MAX : percent);
}

static void format_decimal(char *out, size_t size, double value, int decimals, const char *unit)
{
    char number[24];
    (void)snprintf(number, sizeof(number), "%.*f", decimals, value);
    for (char *p = number; *p; ++p)
        if (*p == '.') *p = ',';
    (void)snprintf(out, size, "%s %s", number, unit);
}

Color battery_level_color(const battery_sample_t *sample)
{
    switch (battery_level(sample)) {
    case BATTERY_LEVEL_CRITICAL: return UI_COLOR_BAD;
    case BATTERY_LEVEL_LOW: return UI_COLOR_ACCENT;
    default: return UI_COLOR_OK;
    }
}

void battery_draw_icon(ui_rect_t rect, int percent, Color color)
{
    const int body_w = rect.w - ICON_NUB_W;
    const ui_rect_t body = {rect.x, rect.y, (int16_t)body_w, rect.h};
    ui_fill(body, UI_COLOR_PANEL);
    ui_outline(body, ICON_BORDER > rect.h / 4 ? rect.h / 4 : ICON_BORDER, UI_COLOR_LINE);
    const int nub_h = rect.h / 2;
    ui_fill((ui_rect_t){(int16_t)(rect.x + body_w), (int16_t)(rect.y + (rect.h - nub_h) / 2),
                        ICON_NUB_W, (int16_t)nub_h}, UI_COLOR_LINE);
    const int inset = ICON_BORDER + (rect.h > 40 ? ICON_INSET : 1);
    const int room = body_w - 2 * inset;
    const int fill = room * clamp_percent(percent) / PERCENT_MAX;
    if (fill > 0)
        ui_fill((ui_rect_t){(int16_t)(rect.x + inset), (int16_t)(rect.y + inset), (int16_t)fill,
                            (int16_t)(rect.h - 2 * inset)}, color);
}

void battery_short_text(const battery_sample_t *sample, char *out, int size)
{
    const battery_state_t state = battery_state(sample);
    if (state == BATTERY_UNKNOWN || state == BATTERY_ABSENT) {
        (void)snprintf(out, (size_t)size, "-");
        return;
    }
    (void)snprintf(out, (size_t)size, "%s%d %%", state == BATTERY_CHARGING ? "+" : "",
                   clamp_percent(sample->percent));
}

static const char *state_text(const battery_sample_t *sample)
{
    switch (battery_state(sample)) {
    case BATTERY_ABSENT: return i18n_text(TXT_BATT_ABSENT);
    case BATTERY_CHARGING: return i18n_text(TXT_BATT_CHARGING);
    case BATTERY_DISCHARGING: return i18n_text(TXT_BATT_DISCHARGING);
    case BATTERY_IDLE:
        return sample->percent >= BATTERY_FULL_PERCENT ? i18n_text(TXT_BATT_FULL) : i18n_text(TXT_BATT_IDLE);
    default: return i18n_text(TXT_BATT_NO_DATA);
    }
}

static void refresh(battery_screen_t *state)
{
    for (int line = 0; line < BATTERY_LINE_COUNT; ++line) ui_lines_printf(&state->view, line, "%s", "");
    const battery_sample_t *sample = &state->sample;
    const battery_state_t kind = battery_state(sample);
    ui_lines_printf(&state->view, BATTERY_LINE_STATE, "%s", state_text(sample));
    if (kind == BATTERY_UNKNOWN || kind == BATTERY_ABSENT) {
        ui_lines_printf(&state->view, BATTERY_LINE_PERCENT, "-");
        return;
    }
    char text[UI_LINE_MAX];
    ui_lines_printf(&state->view, BATTERY_LINE_PERCENT, "%d %%", clamp_percent(sample->percent));
    format_decimal(text, sizeof(text), sample->voltage_mv / 1000.0, 2, "V");
    ui_lines_printf(&state->view, BATTERY_LINE_VOLTAGE, "%s", text);
    ui_lines_printf(&state->view, BATTERY_LINE_CURRENT, "%+d mA", sample->current_ma);
    format_decimal(text, sizeof(text), sample->temperature_dc / 10.0, 1, "°C");
    ui_lines_printf(&state->view, BATTERY_LINE_TEMPERATURE, "%s", text);
    uint16_t minutes;
    if (battery_minutes_remaining(sample, &minutes)) {
        if (minutes >= 60)
            ui_lines_printf(&state->view, BATTERY_LINE_TIME, "%d %s %02d %s", minutes / 60,
                            i18n_text(TXT_BATT_HOURS), minutes % 60, i18n_text(TXT_BATT_MINUTES));
        else
            ui_lines_printf(&state->view, BATTERY_LINE_TIME, "%d %s", minutes, i18n_text(TXT_BATT_MINUTES));
    }
    const battery_level_t level = battery_level(sample);
    if (kind != BATTERY_CHARGING && level == BATTERY_LEVEL_CRITICAL)
        ui_lines_printf(&state->view, BATTERY_LINE_WARNING, "%s", i18n_text(TXT_BATT_CRITICAL));
    else if (kind != BATTERY_CHARGING && level == BATTERY_LEVEL_LOW)
        ui_lines_printf(&state->view, BATTERY_LINE_WARNING, "%s", i18n_text(TXT_BATT_LOW));
}

void battery_screen_relocalize(battery_screen_t *state)
{
    refresh(state);
}

void battery_screen_init(battery_screen_t *state)
{
    memset(state, 0, sizeof(*state));
    ui_lines_init(&state->view, BATTERY_LINE_COUNT);
    refresh(state);
}

void battery_screen_set(battery_screen_t *state, const battery_sample_t *sample)
{
    state->sample = *sample;
    state->sample.percent = (uint8_t)clamp_percent(sample->percent);
    refresh(state);
}

static void draw_row(int index, text_id_t label_id, const char *value)
{
    const int y = ROWS_TOP + index * ROW_PITCH;
    ui_text_draw(i18n_text(label_id), ROW_LABEL_X, y, ROW_SCALE, UI_COLOR_MUTED);
    ui_text_draw_right(value, ROW_VALUE_RIGHT, y, ROW_SCALE, UI_COLOR_TEXT);
}

void battery_screen_draw(const battery_screen_t *state)
{
    const battery_sample_t *sample = &state->sample;
    const battery_state_t kind = battery_state(sample);
    const bool known = kind != BATTERY_UNKNOWN && kind != BATTERY_ABSENT;
    const Color color = battery_level_color(sample);
    battery_draw_icon((ui_rect_t){ICON_X, ICON_Y, ICON_W, ICON_H}, known ? sample->percent : 0, color);
    ui_text_draw_center(ui_lines_get(&state->view, BATTERY_LINE_PERCENT), 240, PERCENT_Y, PERCENT_SCALE,
                        known ? color : UI_COLOR_MUTED);
    ui_text_draw_center(ui_lines_get(&state->view, BATTERY_LINE_STATE), 240, STATE_Y, STATE_SCALE,
                        kind == BATTERY_CHARGING ? UI_COLOR_OK : UI_COLOR_TEXT);
    if (!known) return;
    int row = 0;
    draw_row(row++, TXT_BATT_VOLTAGE, ui_lines_get(&state->view, BATTERY_LINE_VOLTAGE));
    draw_row(row++, TXT_BATT_CURRENT, ui_lines_get(&state->view, BATTERY_LINE_CURRENT));
    draw_row(row++, TXT_BATT_TEMPERATURE, ui_lines_get(&state->view, BATTERY_LINE_TEMPERATURE));
    const char *time = ui_lines_get(&state->view, BATTERY_LINE_TIME);
    if (time && *time)
        draw_row(row, kind == BATTERY_CHARGING ? TXT_BATT_TIME_TO_FULL : TXT_BATT_TIME_LEFT, time);
    const char *warning = ui_lines_get(&state->view, BATTERY_LINE_WARNING);
    if (warning && *warning) ui_text_draw_center(warning, 240, WARNING_Y, 2, color);
}
