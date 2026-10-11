// SPDX-License-Identifier: Apache-2.0
/* What the bench assistant remembers across restarts: the language and what
 * was last entered on every screen. A flat blob of bytes (no pointers, no
 * padding) so the device can keep it in NVS unchanged; layout changes need a
 * new SETTINGS_VERSION. Values read back are never trusted: game.c rebuilds
 * every screen through the normal entry functions and ignores what is invalid. */
#pragma once
#include <stdint.h>
#include "calc.h"
#include "screen_level.h"

#define SETTINGS_VERSION 2
#define SETTINGS_ENTRY_SIZE (CALC_ENTRY_MAX + 1)

enum { SETTINGS_OHM_ROWS = 4, SETTINGS_CABLE_FIELDS = 3 };

typedef struct {
    uint8_t language;
    uint8_t conv_qty, conv_unit;
    uint8_t ohm_unit[SETTINGS_OHM_ROWS];
    uint8_t ohm_unit_auto[SETTINGS_OHM_ROWS];
    uint8_t ohm_selected;
    uint8_t cable_material, cable_system, cable_selected;
    uint8_t color_bands, color_selected;
    uint8_t color_color[RESISTOR_MAX_BANDS];
    uint8_t level_mode;
    uint8_t power_brightness;           /* Screen brightness level, see screen_power.h. */
    uint8_t power_sleep;                /* Screen-off timeout choice. */
    uint8_t reserved;                   /* Keeps the 16-bit fields below aligned. */
    int16_t level_zero[LEVEL_MODE_COUNT][LEVEL_AXES];
    char conv_entry[SETTINGS_ENTRY_SIZE];
    char ohm_entry[SETTINGS_OHM_ROWS][SETTINGS_ENTRY_SIZE];
    char cable_entry[SETTINGS_CABLE_FIELDS][SETTINGS_ENTRY_SIZE];
} settings_t;

/* 26 bytes of flags, 8 of level offsets, 88 of entry text. A different size means the
 * compiler added padding. */
_Static_assert(sizeof(settings_t) == 122, "settings_t must have no padding");
_Static_assert(sizeof(settings_t) <= 256, "must fit one save blob (MOSAICO_SAVE_MAX_PAYLOAD)");
