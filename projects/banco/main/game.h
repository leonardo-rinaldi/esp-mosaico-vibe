// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "power_status.h"
#include "settings.h"

typedef struct game_t *game_handle_t;
typedef struct {
    uint16_t width;
    uint16_t height;
} game_config_t;

/* GAME_SCREEN_MENU is the home screen; its tiles open the nine functions, in this order. */
typedef enum {
    GAME_SCREEN_MENU,
    GAME_SCREEN_CONVERTER,
    GAME_SCREEN_OHM,
    GAME_SCREEN_COLORS,
    GAME_SCREEN_CABLE,
    GAME_SCREEN_LEVEL,
    GAME_SCREEN_LED,
    GAME_SCREEN_DIVIDER,
    GAME_SCREEN_SMD,
    GAME_SCREEN_AWG,
    GAME_SCREEN_BATTERY,
    GAME_SCREEN_POWER,
    GAME_SCREEN_COUNT,
} game_screen_t;

typedef struct {
    uint32_t tick;
    int32_t pointer_x;
    int32_t pointer_y;
    bool pointer_down;
    bool paused;
    game_screen_t screen;
    uint32_t view_hash;     /* Covers every text line of the active screen. */
} game_snapshot_t;

/* Portable C shared by Host and device. One loop owns each instance and calls
 * these methods; input adapters enqueue or deliver events on that same loop.
 * A touch acts when the finger goes down. */
bool game_create(const game_config_t *config, game_handle_t *ret_handle);
void game_delete(game_handle_t handle);
void game_reset(game_handle_t handle);
void game_set_paused(game_handle_t handle, bool paused);

/* Remembered values (see settings.h). poll returns true once after every change
 * with the new values; apply restores them, ignoring anything invalid, and does
 * not count as a change. */
void game_settings_apply(game_handle_t handle, const settings_t *settings);
bool game_settings_poll(game_handle_t handle, settings_t *settings);
void game_settings_get(game_handle_t handle, settings_t *settings);
/* Latest battery reading from the fuel gauge; the menu header and the battery screen show it. */
void game_set_battery(game_handle_t handle, const battery_sample_t *sample);
/* Screen brightness the display should have now, in percent: the chosen level, lowered after
 * a while without a touch, and 0 once the screen has turned off (nothing is drawn then).
 * The board applies it to the panel whenever it changes. */
int game_display_brightness(game_handle_t handle);
void game_set_pointer(game_handle_t handle, int32_t x, int32_t y, bool pressed);
void game_update(game_handle_t handle);
bool game_render(game_handle_t handle);
game_snapshot_t game_read(game_handle_t handle);

/* The text the active screen shows, line by line, for automation and tests.
 * Pointers stay valid until the next input. Returns NULL past the last line. */
int game_line_count(game_handle_t handle);
const char *game_line(game_handle_t handle, int index);
const char *game_screen_name(game_screen_t screen);
