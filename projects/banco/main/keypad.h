// SPDX-License-Identifier: Apache-2.0
/* The numeric keypad shared by the screens that take typed numbers. It fills the
 * bottom 160 pixels: four columns of 120 pixels, four rows of 40. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

enum { KEYPAD_TOP = 320, KEYPAD_ROW_HEIGHT = 40, KEYPAD_COLUMN_WIDTH = 120 };

typedef enum {
    KEYPAD_NONE = -1,
    KEYPAD_0, KEYPAD_1, KEYPAD_2, KEYPAD_3, KEYPAD_4,
    KEYPAD_5, KEYPAD_6, KEYPAD_7, KEYPAD_8, KEYPAD_9,
    KEYPAD_DOUBLE_ZERO,
    KEYPAD_POINT,
    KEYPAD_BACKSPACE,
    KEYPAD_CLEAR,
    KEYPAD_SIGN,
} keypad_key_t;

void keypad_draw(const ui_pointer_t *pointer);
keypad_key_t keypad_hit(int32_t x, int32_t y);
/* Edits the entry. Returns true when the touch landed on a key. */
bool keypad_apply(calc_entry_t *entry, int32_t x, int32_t y);
