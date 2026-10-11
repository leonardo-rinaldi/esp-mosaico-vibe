// SPDX-License-Identifier: Apache-2.0
/* Battery screen: charge level, whether it is charging, and the gauge's numbers.
 * Opened by tapping the battery indicator in the menu header. */
#pragma once
#include "power_status.h"
#include "ui_widgets.h"

/* View lines: 0 percent ("78 %", or "-"), 1 the state text, 2 voltage, 3 current,
 * 4 temperature, 5 time left or to full (empty when unknown), 6 the warning. */
enum {
    BATTERY_LINE_PERCENT, BATTERY_LINE_STATE, BATTERY_LINE_VOLTAGE, BATTERY_LINE_CURRENT,
    BATTERY_LINE_TEMPERATURE, BATTERY_LINE_TIME, BATTERY_LINE_WARNING, BATTERY_LINE_COUNT,
};

typedef struct {
    battery_sample_t sample;
    ui_lines_t view;
} battery_screen_t;

void battery_screen_init(battery_screen_t *state);
void battery_screen_set(battery_screen_t *state, const battery_sample_t *sample);
void battery_screen_relocalize(battery_screen_t *state);
void battery_screen_draw(const battery_screen_t *state);

/* Colour that matches the level: green, amber when low, red when critical. */
Color battery_level_color(const battery_sample_t *sample);
/* A battery outline filled to `percent` (0..100), for the menu header and this screen. */
void battery_draw_icon(ui_rect_t rect, int percent, Color color);
/* The short text under the icon in the menu header: "78 %", "+78 %" while charging, "-". */
void battery_short_text(const battery_sample_t *sample, char *out, int size);
