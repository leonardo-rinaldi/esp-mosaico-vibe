// SPDX-License-Identifier: Apache-2.0
#include "screen_smd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "calc_bench.h"
#include "form.h"
#include "i18n.h"
#include "keypad.h"
#include "ui_font.h"

enum {
    MODES_TOP = 48, MODES_HEIGHT = 40, CODE_TOP = 92, CODE_HEIGHT = 56, LETTERS_TOP = 152, LETTERS_HEIGHT = 36,
    RESULT_TOP = 192, RESULT_HEIGHT = 122, BIG_SCALE = 4, CODE_SCALE = 5, ROW_SCALE = 2, LABEL_SCALE = 2,
    PICO_PER_FARAD_EXP = 12,
};

static const char s_letter[SMD_LETTER_COUNT] = {'Z', 'Y', 'X', 'A', 'B', 'C', 'D', 'E', 'F'};
static const int s_max_digits[SMD_MODE_COUNT] = {4, 2, 3};
enum { DEFAULT_LETTER = 3 };            /* 'A', multiplier 1. */

static void apply_key(smd_state_t *state, keypad_key_t key)
{
    char *code = state->code;
    size_t length = strlen(code);
    const size_t max_digits = (size_t)s_max_digits[state->mode];
    size_t digits = length;
    if (strchr(code, ',')) --digits;
    if (key >= KEYPAD_0 && key <= KEYPAD_9) {
        if (digits < max_digits) {
            code[length] = (char)('0' + (key - KEYPAD_0));
            code[length + 1] = '\0';
        }
    } else if (key == KEYPAD_DOUBLE_ZERO) {
        if (digits + 2 <= max_digits) {
            code[length] = '0';
            code[length + 1] = '0';
            code[length + 2] = '\0';
        }
    } else if (key == KEYPAD_POINT) {
        if (state->mode != SMD_MODE_E96 && !strchr(code, ',')) {
            code[length] = ',';
            code[length + 1] = '\0';
        }
    } else if (key == KEYPAD_BACKSPACE) {
        if (length > 0) code[length - 1] = '\0';
    } else if (key == KEYPAD_CLEAR) {
        code[0] = '\0';
    }
}

static void format_value(smd_mode_t mode, double base, char *out, size_t size)
{
    if (base == 0.0) {
        (void)snprintf(out, size, mode == SMD_MODE_CAPACITOR ? "0 pF" : "0 Ω");
        return;
    }
    form_format_quantity(mode == SMD_MODE_CAPACITOR ? CALC_QTY_CAPACITANCE : CALC_QTY_RESISTANCE, base, out, size);
}

static void refresh(smd_state_t *state)
{
    const char *text = state->code;
    char code[UI_LINE_MAX];
    size_t length = 0;
    for (const char *p = text; *p && length + 2 < sizeof(code); ++p) code[length++] = *p == ',' ? 'R' : *p;
    if (state->mode == SMD_MODE_E96 && length > 0 && length + 2 < sizeof(code)) code[length++] = s_letter[state->letter];
    code[length] = '\0';
    ui_lines_printf(&state->view, SMD_LINE_CODE, "%s", code);
    ui_lines_printf(&state->view, SMD_LINE_VALUE, "%s", "");
    ui_lines_printf(&state->view, SMD_LINE_EXTRA, "%s", "");
    ui_lines_printf(&state->view, SMD_LINE_HINT, "%s", "");
    ui_lines_printf(&state->view, SMD_LINE_NOTE, "%s", state->mode == SMD_MODE_E96 ? "" : i18n_text(TXT_SMD_KEY_R));
    state->valid = false;

    double value = 0.0;
    smd_status_t status;
    if (state->mode == SMD_MODE_RESISTOR) status = smd_resistor(text, &value);
    else if (state->mode == SMD_MODE_CAPACITOR) status = smd_capacitor(text, &value);
    else status = smd_eia96(text, s_letter[state->letter], &value);
    if (status != SMD_OK) {
        ui_lines_printf(&state->view, SMD_LINE_HINT, "%s",
                        i18n_text(status == SMD_INVALID ? TXT_SMD_INVALID : TXT_SMD_ENTER));
        return;
    }
    char shown[UI_LINE_MAX];
    format_value(state->mode, state->mode == SMD_MODE_CAPACITOR ? value * pow(10.0, -PICO_PER_FARAD_EXP) : value,
                 shown, sizeof(shown));
    ui_lines_printf(&state->view, SMD_LINE_VALUE, "%s", shown);
    if (state->mode == SMD_MODE_E96) {
        char base[24], multiplier[24];
        (void)calc_format(eia96_base((text[0] - '0') * 10 + (text[1] - '0')), base, sizeof(base));
        (void)calc_format(eia96_multiplier(s_letter[state->letter]), multiplier, sizeof(multiplier));
        ui_lines_printf(&state->view, SMD_LINE_EXTRA, "%s X %s", base, multiplier);
    }
    state->valid = true;
}

void smd_relocalize(smd_state_t *state)
{
    refresh(state);
}

void smd_init(smd_state_t *state)
{
    ui_lines_init(&state->view, SMD_LINE_COUNT);
    state->mode = SMD_MODE_RESISTOR;
    state->letter = DEFAULT_LETTER;
    state->code[0] = '\0';
    refresh(state);
}

static ui_rect_t mode_rect(int index)
{
    return form_button_rect(MODES_TOP, MODES_HEIGHT, SMD_MODE_COUNT, index);
}

static ui_rect_t letter_rect(int index)
{
    return form_button_rect(LETTERS_TOP, LETTERS_HEIGHT, SMD_LETTER_COUNT, index);
}

void smd_tap(smd_state_t *state, int32_t x, int32_t y)
{
    for (int m = 0; m < SMD_MODE_COUNT; ++m) {
        if (ui_rect_contains(mode_rect(m), x, y)) {
            state->mode = (smd_mode_t)m;
            state->code[0] = '\0';
            refresh(state);
            return;
        }
    }
    if (state->mode == SMD_MODE_E96) {
        for (int i = 0; i < SMD_LETTER_COUNT; ++i) {
            if (ui_rect_contains(letter_rect(i), x, y)) {
                state->letter = i;
                refresh(state);
                return;
            }
        }
    }
    const keypad_key_t key = keypad_hit(x, y);
    if (key != KEYPAD_NONE) {
        apply_key(state, key);
        refresh(state);
    }
}

void smd_draw(const smd_state_t *state, const ui_pointer_t *pointer)
{
    for (int m = 0; m < SMD_MODE_COUNT; ++m) {
        const char *label = m == SMD_MODE_E96 ? "E96"
                            : i18n_text(m == SMD_MODE_RESISTOR ? TXT_SMD_RESISTOR : TXT_SMD_CAPACITOR);
        ui_button(mode_rect(m), label, LABEL_SCALE,
                  state->mode == (smd_mode_t)m ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    }
    const ui_rect_t code = {6, CODE_TOP, 468, CODE_HEIGHT};
    ui_fill(code, UI_COLOR_PANEL);
    ui_outline(code, 3, UI_COLOR_ACCENT);
    const char *text = ui_lines_get(&state->view, SMD_LINE_CODE);
    ui_text_draw_center(*text ? text : "-", 240, code.y + (code.h - ui_text_height(CODE_SCALE)) / 2, CODE_SCALE,
                        *text ? UI_COLOR_TEXT : UI_COLOR_MUTED);
    if (state->mode == SMD_MODE_E96) {
        for (int i = 0; i < SMD_LETTER_COUNT; ++i) {
            const char label[2] = {s_letter[i], '\0'};
            ui_button(letter_rect(i), label, LABEL_SCALE,
                      state->letter == i ? UI_BUTTON_SELECTED : UI_BUTTON_FUNCTION, pointer);
        }
    }
    const ui_rect_t box = {6, RESULT_TOP, 468, RESULT_HEIGHT};
    ui_fill(box, state->valid ? UI_COLOR_OK_BG : UI_COLOR_PANEL);
    if (state->valid) {
        ui_text_draw_center(ui_lines_get(&state->view, SMD_LINE_VALUE), 240, box.y + 16, BIG_SCALE, UI_COLOR_OK);
        ui_text_draw_center(ui_lines_get(&state->view, SMD_LINE_EXTRA), 240, box.y + 60, ROW_SCALE, UI_COLOR_TEXT);
    } else {
        const char *hint = ui_lines_get(&state->view, SMD_LINE_HINT);
        ui_text_draw_center(hint, 240, box.y + 16, ROW_SCALE,
                            hint && strcmp(hint, i18n_text(TXT_SMD_INVALID)) == 0 ? UI_COLOR_BAD : UI_COLOR_MUTED);
    }
    ui_text_draw_center(ui_lines_get(&state->view, SMD_LINE_NOTE), 240, box.y + box.h - 26, ROW_SCALE,
                        UI_COLOR_MUTED);
    keypad_draw(pointer);
}
