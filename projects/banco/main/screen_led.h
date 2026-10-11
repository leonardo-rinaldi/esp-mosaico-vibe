// SPDX-License-Identifier: Apache-2.0
/* LED series resistor: from the supply voltage, the LED's forward voltage and the wanted
 * current, find the resistor (next E24 value up) and what it dissipates. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

enum { LED_FIELD_SUPPLY, LED_FIELD_LED, LED_FIELD_CURRENT, LED_FIELD_COUNT };

/* View lines: 0 the E24 resistor ("150 Ω"), 1 the exact value, 2 the real current, 3 the
 * power, 4 the power rating to use ("1/4 W"), 5 the hint or error, 6..8 the typed supply
 * voltage, LED voltage and current. Lines 0..4 are empty until the inputs are valid. */
enum {
    LED_LINE_STANDARD, LED_LINE_EXACT, LED_LINE_ACTUAL, LED_LINE_POWER, LED_LINE_RATING,
    LED_LINE_HINT, LED_LINE_ENTRIES, LED_LINE_COUNT = LED_LINE_ENTRIES + LED_FIELD_COUNT,
};

typedef struct {
    calc_entry_t entry[LED_FIELD_COUNT];
    int selected;
    bool valid;
    ui_lines_t view;
} led_state_t;

void led_init(led_state_t *state);
void led_relocalize(led_state_t *state);
void led_tap(led_state_t *state, int32_t x, int32_t y);
void led_draw(const led_state_t *state, const ui_pointer_t *pointer);
