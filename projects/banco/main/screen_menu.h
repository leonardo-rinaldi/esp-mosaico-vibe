// SPDX-License-Identifier: Apache-2.0
/* Home screen: five large tiles (two columns; an odd last tile spans both), one per function. Tapping a tile opens that
 * function; the tile order is the order of the function screens in game.h. */
#pragma once
#include "i18n.h"
#include "power_status.h"
#include "ui_widgets.h"

enum { MENU_TILE_COUNT = 5 };

/* View lines: one title per tile in tile order, then the battery text of the header. */
enum { MENU_LINE_BATTERY = MENU_TILE_COUNT, MENU_LINE_COUNT };

typedef struct {
    battery_sample_t battery;
    ui_lines_t view;
} menu_state_t;

void menu_init(menu_state_t *state);
/* Rewrite the tile titles in the current language. */
void menu_relocalize(menu_state_t *state);
void menu_set_battery(menu_state_t *state, const battery_sample_t *sample);
/* True when (x, y) is on the battery indicator in the header. */
bool menu_battery_hit(int32_t x, int32_t y);
/* Index of the tile under (x, y), or -1 when the touch is between or outside tiles. */
int menu_hit(int32_t x, int32_t y);
/* Language button under (x, y), or -1. */
int menu_language_hit(int32_t x, int32_t y);
void menu_draw(const menu_state_t *state, const ui_pointer_t *pointer);
