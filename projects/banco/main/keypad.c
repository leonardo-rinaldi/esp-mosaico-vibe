// SPDX-License-Identifier: Apache-2.0
#include "keypad.h"
#include <stddef.h>

enum { KEY_LABEL_SCALE = 3 };

typedef struct {
    uint8_t column, row, span;
    keypad_key_t key;
    const char *label;
} key_def_t;

static const key_def_t s_keys[] = {
    {0, 0, 1, KEYPAD_7, "7"}, {1, 0, 1, KEYPAD_8, "8"}, {2, 0, 1, KEYPAD_9, "9"},
    {3, 0, 1, KEYPAD_BACKSPACE, "<"},
    {0, 1, 1, KEYPAD_4, "4"}, {1, 1, 1, KEYPAD_5, "5"}, {2, 1, 1, KEYPAD_6, "6"},
    {3, 1, 1, KEYPAD_CLEAR, "C"},
    {0, 2, 1, KEYPAD_1, "1"}, {1, 2, 1, KEYPAD_2, "2"}, {2, 2, 1, KEYPAD_3, "3"},
    {3, 2, 1, KEYPAD_SIGN, "±"},
    {0, 3, 2, KEYPAD_0, "0"}, {2, 3, 1, KEYPAD_POINT, ","}, {3, 3, 1, KEYPAD_DOUBLE_ZERO, "00"},
};

static ui_rect_t key_rect(const key_def_t *key)
{
    return (ui_rect_t){
        (int16_t)(key->column * KEYPAD_COLUMN_WIDTH),
        (int16_t)(KEYPAD_TOP + key->row * KEYPAD_ROW_HEIGHT),
        (int16_t)(key->span * KEYPAD_COLUMN_WIDTH), KEYPAD_ROW_HEIGHT,
    };
}

static bool is_editing_key(keypad_key_t key)
{
    return key == KEYPAD_BACKSPACE || key == KEYPAD_CLEAR || key == KEYPAD_SIGN;
}

void keypad_draw(const ui_pointer_t *pointer)
{
    for (size_t i = 0; i < sizeof(s_keys) / sizeof(s_keys[0]); ++i) {
        ui_button(key_rect(&s_keys[i]), s_keys[i].label, KEY_LABEL_SCALE,
                  is_editing_key(s_keys[i].key) ? UI_BUTTON_FUNCTION : UI_BUTTON_NORMAL, pointer);
    }
}

keypad_key_t keypad_hit(int32_t x, int32_t y)
{
    for (size_t i = 0; i < sizeof(s_keys) / sizeof(s_keys[0]); ++i)
        if (ui_rect_contains(key_rect(&s_keys[i]), x, y)) return s_keys[i].key;
    return KEYPAD_NONE;
}

bool keypad_apply(calc_entry_t *entry, int32_t x, int32_t y)
{
    const keypad_key_t key = keypad_hit(x, y);
    switch (key) {
    case KEYPAD_NONE: return false;
    case KEYPAD_DOUBLE_ZERO: (void)calc_entry_double_zero(entry); break;
    case KEYPAD_POINT: (void)calc_entry_point(entry); break;
    case KEYPAD_BACKSPACE: (void)calc_entry_backspace(entry); break;
    case KEYPAD_CLEAR: calc_entry_clear(entry); break;
    case KEYPAD_SIGN: (void)calc_entry_toggle_sign(entry); break;
    default: (void)calc_entry_digit(entry, (char)('0' + (int)key)); break;
    }
    return true;
}
