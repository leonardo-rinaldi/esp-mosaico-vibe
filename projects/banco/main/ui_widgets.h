// SPDX-License-Identifier: Apache-2.0
/* Shared look and building blocks of the bench assistant screens. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "mosaico_raylib_fast.h"

#define UI_COLOR_BG ((Color){14, 18, 28, 255})
#define UI_COLOR_PANEL ((Color){28, 36, 54, 255})
#define UI_COLOR_KEY ((Color){40, 52, 76, 255})
#define UI_COLOR_KEY_FN ((Color){60, 76, 110, 255})
#define UI_COLOR_KEY_DOWN ((Color){120, 140, 190, 255})
#define UI_COLOR_TEXT ((Color){235, 240, 250, 255})
#define UI_COLOR_MUTED ((Color){140, 155, 185, 255})
#define UI_COLOR_ACCENT ((Color){255, 176, 0, 255})
#define UI_COLOR_ON_ACCENT ((Color){20, 20, 20, 255})
#define UI_COLOR_OK ((Color){80, 220, 120, 255})
#define UI_COLOR_OK_BG ((Color){30, 70, 50, 255})
#define UI_COLOR_BAD ((Color){240, 90, 90, 255})
#define UI_COLOR_LINE ((Color){70, 90, 120, 255})

enum {
    UI_TAB_HEIGHT = 44,
    UI_LINE_MAX = 40,           /* Bytes per text line, including the terminator. */
    UI_LINES_MAX = 10,
    UI_BLINK_TICKS = 15,        /* Entry cursor blink half-period. */
};

typedef struct {
    int16_t x, y, w, h;
} ui_rect_t;

/* Latest touch sample: where it is and whether the finger is down. */
typedef struct {
    int32_t x, y;
    bool down;
} ui_pointer_t;

typedef enum {
    UI_BUTTON_NORMAL,
    UI_BUTTON_FUNCTION,         /* Editing keys. */
    UI_BUTTON_SELECTED,
    UI_BUTTON_DISABLED,
} ui_button_style_t;

/* The display text of one screen, in the order its header documents. Screens
 * refresh it after every input; drawing and the Host state read the same strings. */
typedef struct {
    char text[UI_LINES_MAX][UI_LINE_MAX];
    uint8_t count;
} ui_lines_t;

bool ui_rect_contains(ui_rect_t rect, int32_t x, int32_t y);
bool ui_pointer_inside(const ui_pointer_t *pointer, ui_rect_t rect);
void ui_fill(ui_rect_t rect, Color color);
void ui_outline(ui_rect_t rect, int thickness, Color color);
void ui_button(ui_rect_t rect, const char *label, int scale, ui_button_style_t style,
               const ui_pointer_t *pointer);

void ui_lines_init(ui_lines_t *lines, int count);
void ui_lines_printf(ui_lines_t *lines, int index, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
const char *ui_lines_get(const ui_lines_t *lines, int index);
