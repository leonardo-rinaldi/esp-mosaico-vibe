// SPDX-License-Identifier: Apache-2.0
/* Power saving: how bright the screen is, and when it dims and then turns off after the last
 * touch. The display is the board's biggest consumer, and the interface is dark, so a lower
 * brightness and a short timeout are what make the battery last. Opened from the battery
 * screen. The idle timing itself runs in game.c; this screen only holds the two choices. */
#pragma once
#include "ui_widgets.h"

enum { POWER_BRIGHTNESS_COUNT = 3, POWER_SLEEP_COUNT = 4 };

/* View lines: 0 the chosen brightness name, 1 the chosen timeout name. */
enum { POWER_LINE_BRIGHTNESS, POWER_LINE_SLEEP, POWER_LINE_COUNT };

typedef struct {
    uint8_t brightness;             /* Index: 0 high, 1 medium, 2 low. */
    uint8_t sleep;                  /* Index: 0 never, 1 30 s, 2 1 min, 3 2 min. */
    ui_lines_t view;
} power_state_t;

void power_init(power_state_t *state);
void power_relocalize(power_state_t *state);
void power_tap(power_state_t *state, int32_t x, int32_t y);
void power_draw(const power_state_t *state, const ui_pointer_t *pointer);
/* Screen brightness in percent for the chosen level. */
int power_brightness_percent(const power_state_t *state);
/* Seconds without a touch before the screen turns off; 0 means never. */
int power_sleep_seconds(const power_state_t *state);
