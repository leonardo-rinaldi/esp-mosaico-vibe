// SPDX-License-Identifier: Apache-2.0
/* Unit converter: type a value, pick its unit, read it in every unit of the
 * same quantity at once. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

/* View lines: 0..3 the value in each unit (table order), 4 the typed entry. */
enum { CONV_LINE_ENTRY = CALC_UNITS_PER_QTY };

typedef struct {
    calc_qty_t qty;
    int unit;               /* Unit the typed value is in. */
    calc_entry_t entry;
    int best;               /* Row of the most readable unit, -1 while empty. */
    ui_lines_t view;
} conv_state_t;

void conv_init(conv_state_t *state);
/* Recompute the lines after the state was restored from saved settings. */
void conv_relocalize(conv_state_t *state);
void conv_tap(conv_state_t *state, int32_t x, int32_t y);
void conv_draw(const conv_state_t *state, const ui_pointer_t *pointer, uint32_t tick);
