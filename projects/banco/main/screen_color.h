// SPDX-License-Identifier: Apache-2.0
/* Resistor colour code: pick the colour of each band, read the value. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

/* View lines: 0 resistance ("4,7 kΩ", or "-"), 1 tolerance ("±5 %"),
 * 2 what the selected band means ("BANDA 3: MOLTIPLICATORE"). */
enum { COLOR_LINE_VALUE, COLOR_LINE_TOLERANCE, COLOR_LINE_BAND, COLOR_LINE_COUNT };

typedef struct {
    int bands;                              /* 4 or 5. */
    calc_color_t color[RESISTOR_MAX_BANDS];
    int selected;                           /* Band the palette sets. */
    ui_lines_t view;
} color_state_t;

void color_init(color_state_t *state);
void color_tap(color_state_t *state, int32_t x, int32_t y);
void color_draw(const color_state_t *state, const ui_pointer_t *pointer);
