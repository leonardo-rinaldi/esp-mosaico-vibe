// SPDX-License-Identifier: Apache-2.0
/* SMD part codes. RESISTOR: 3 or 4 digits ("103" = 10 kΩ, "1002" = 10 kΩ) or R notation
 * with the "," key ("4R7" = 4,7 Ω). E96: two digits and a multiplier letter ("68X" =
 * 49,9 Ω). CAPACITOR: 3 digits in pF ("104" = 100 nF) or R notation. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

typedef enum { SMD_MODE_RESISTOR, SMD_MODE_E96, SMD_MODE_CAPACITOR, SMD_MODE_COUNT } smd_mode_t;

enum { SMD_LETTER_COUNT = 9 };

/* View lines: 0 the code as printed on the part ("4R7", "68X"), 1 the value ("10 kΩ"),
 * 2 the extra line (E96: the base value and multiplier), 3 the hint or error, 4 the note
 * about the "," key. Lines 1 and 2 are empty until the code is complete and valid. */
enum { SMD_LINE_CODE, SMD_LINE_VALUE, SMD_LINE_EXTRA, SMD_LINE_HINT, SMD_LINE_NOTE, SMD_LINE_COUNT };

/* The code is kept as typed (digits and one ',' for the R), not as a number: leading zeros
 * matter here ("000", "01A"). */
enum { SMD_CODE_MAX = 5 };

typedef struct {
    smd_mode_t mode;
    char code[SMD_CODE_MAX + 1];
    int letter;                         /* Index into the multiplier letters (E96 mode). */
    bool valid;
    ui_lines_t view;
} smd_state_t;

void smd_init(smd_state_t *state);
void smd_relocalize(smd_state_t *state);
void smd_tap(smd_state_t *state, int32_t x, int32_t y);
void smd_draw(const smd_state_t *state, const ui_pointer_t *pointer);
