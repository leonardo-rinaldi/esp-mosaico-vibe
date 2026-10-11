// SPDX-License-Identifier: Apache-2.0
/* Ohm's law and power: type any two of V, I, R, P and the other two are
 * calculated. Each value has its own unit. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

/* View lines: 0..3 value of V, I, R, P (typed text, calculated result, or empty),
 * 4 the hint or error, 5..8 the unit symbol shown beside each value. */
enum { OHM_LINE_HINT = OHM_COUNT, OHM_LINE_UNITS = OHM_COUNT + 1 };

typedef struct {
    calc_entry_t entry[OHM_COUNT];
    int unit[OHM_COUNT];
    bool unit_auto[OHM_COUNT];      /* A calculated value picks its own readable unit. */
    int selected;                   /* Row the keypad edits. */
    bool known[OHM_COUNT];
    bool computed[OHM_COUNT];
    int known_count;
    ohm_status_t status;
    ui_lines_t view;
} ohm_state_t;

void ohm_init(ohm_state_t *state);
/* Rebuild the texts that depend on the language, keeping the entered values. */
void ohm_relocalize(ohm_state_t *state);
void ohm_tap(ohm_state_t *state, int32_t x, int32_t y);
void ohm_draw(const ohm_state_t *state, const ui_pointer_t *pointer);
