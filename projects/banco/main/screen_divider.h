// SPDX-License-Identifier: Apache-2.0
/* Voltage divider. Two modes: FIND VOUT (from VIN, R1, R2) and FIND R2 (from VIN, the wanted
 * VOUT and R1, with the nearest E24 value and the output it would give). One unit button
 * (Ω, kΩ, MΩ) applies to both resistors. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

typedef enum { DIVIDER_FIND_VOUT, DIVIDER_FIND_R2, DIVIDER_MODE_COUNT } divider_mode_t;

enum { DIVIDER_FIELDS = 3, DIVIDER_UNITS = 3 };

/* View lines: 0 the result ("2,5 V" or "19,41 kΩ"), 1 the extra line (current, or the E24
 * value and its output), 2 the hint or error, 3..5 the three typed values of the active mode
 * (VIN, R1, R2 or VIN, VOUT, R1). Lines 0 and 1 are empty until the inputs are valid. */
enum { DIVIDER_LINE_RESULT, DIVIDER_LINE_EXTRA, DIVIDER_LINE_HINT, DIVIDER_LINE_ENTRIES,
       DIVIDER_LINE_COUNT = DIVIDER_LINE_ENTRIES + DIVIDER_FIELDS };

typedef struct {
    divider_mode_t mode;
    int unit;                               /* 0 Ω, 1 kΩ, 2 MΩ. */
    calc_entry_t entry[DIVIDER_MODE_COUNT][DIVIDER_FIELDS];
    int selected;
    bool valid;
    ui_lines_t view;
} divider_state_t;

void divider_init(divider_state_t *state);
void divider_relocalize(divider_state_t *state);
void divider_tap(divider_state_t *state, int32_t x, int32_t y);
void divider_draw(const divider_state_t *state, const ui_pointer_t *pointer);
