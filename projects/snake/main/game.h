// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct game_t *game_handle_t;
typedef struct {
    uint16_t width;
    uint16_t height;
} game_config_t;

/* Clockwise order: the opposite of a direction is (dir + 2) % GAME_DIR_COUNT. */
typedef enum {
    GAME_DIR_UP = 0,
    GAME_DIR_RIGHT,
    GAME_DIR_DOWN,
    GAME_DIR_LEFT,
    GAME_DIR_COUNT,
} game_dir_t;

typedef enum {
    GAME_PHASE_READY = 0,   /* New board shown, waiting for the first input. */
    GAME_PHASE_RUNNING,
    GAME_PHASE_OVER,        /* Crashed, or the board is full (won). */
} game_phase_t;

typedef struct {
    uint32_t tick;
    int32_t pointer_x;
    int32_t pointer_y;
    bool pointer_down;
    bool paused;
    game_phase_t phase;
    bool won;
    game_dir_t dir;
    uint16_t score;
    uint16_t best;          /* Best score since the game instance was created. */
    uint16_t length;
    int16_t head_x;         /* Board cell coordinates. */
    int16_t head_y;
    int16_t food_x;
    int16_t food_y;
    uint32_t body_hash;     /* Covers every segment position, for replay checks. */
} game_snapshot_t;

/* Portable C shared by Host and device. One loop owns each instance and calls
 * these methods; input adapters enqueue or deliver events on that same loop. */
bool game_create(const game_config_t *config, game_handle_t *ret_handle);
void game_delete(game_handle_t handle);
void game_reset(game_handle_t handle);
void game_set_paused(game_handle_t handle, bool paused);
void game_set_pointer(game_handle_t handle, int32_t x, int32_t y, bool pressed);
/* Turns the snake. Also starts a ready game and restarts a finished one. */
void game_set_direction(game_handle_t handle, game_dir_t dir);
void game_update(game_handle_t handle);
bool game_render(game_handle_t handle);
game_snapshot_t game_read(game_handle_t handle);
