// SPDX-License-Identifier: Apache-2.0
/* AWG wire sizes. AWG TO MM²: type an AWG number (the ± key gives 00, 000, 0000 as -1, -2,
 * -3) and read the area, the diameter and the next metric standard section. MM² TO AWG:
 * type an area and read the nearest AWG with its exact area and diameter. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

typedef enum { AWG_MODE_TO_MM2, AWG_MODE_TO_AWG, AWG_MODE_COUNT } awg_mode_t;

/* View lines: 0 the typed value, 1..3 the three result rows (TO MM²: area, diameter, next
 * metric size; TO AWG: the AWG, its area, its diameter), 4 the hint or error, 5 the AWG name
 * shown beside the field ("=00" for -1; empty otherwise). Lines 1..3 are empty until valid. */
enum { AWG_LINE_ENTRY, AWG_LINE_ROW1, AWG_LINE_ROW2, AWG_LINE_ROW3, AWG_LINE_HINT, AWG_LINE_ALIAS,
       AWG_LINE_COUNT };

typedef struct {
    awg_mode_t mode;
    calc_entry_t entry[AWG_MODE_COUNT];
    bool valid;
    ui_lines_t view;
} awg_state_t;

void awg_init(awg_state_t *state);
void awg_relocalize(awg_state_t *state);
void awg_tap(awg_state_t *state, int32_t x, int32_t y);
void awg_draw(const awg_state_t *state, const ui_pointer_t *pointer);
