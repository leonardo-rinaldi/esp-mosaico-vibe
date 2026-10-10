// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include "game.h"
#include "game_config.h"
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"

typedef struct {
    game_handle_t game;
    bool dir_held[GAME_DIR_COUNT];  /* The Host repeats held actions every frame. */
} host_state_t;

static const struct {
    int32_t code;
    game_dir_t dir;
} s_dir_actions[] = {
    {GAME_ACTION_UP, GAME_DIR_UP},
    {GAME_ACTION_RIGHT, GAME_DIR_RIGHT},
    {GAME_ACTION_DOWN, GAME_DIR_DOWN},
    {GAME_ACTION_LEFT, GAME_DIR_LEFT},
};

static const char *phase_name(const game_snapshot_t *state)
{
    if (state->paused) return "paused";
    switch (state->phase) {
    case GAME_PHASE_READY: return "ready";
    case GAME_PHASE_RUNNING: return "running";
    case GAME_PHASE_OVER: return state->won ? "won" : "game_over";
    }
    return "unknown";
}

static int initialize(void *value, const char *asset_root)
{
    (void)asset_root;
    host_state_t *state = value;
    const game_config_t config = {GAME_WIDTH, GAME_HEIGHT};
    if (!game_create(&config, &state->game)) return -1;
    InitWindow(GAME_WIDTH, GAME_HEIGHT, GAME_NAME);
    SetTargetFPS(GAME_TICK_HZ);
    return 0;
}

static void shutdown(void *value)
{
    host_state_t *state = value;
    game_delete(state->game);
    state->game = NULL;
}

/* A turn fires once per key press, not on every repeated "held" event. */
static void direction_action(host_state_t *state, int32_t code, bool pressed)
{
    for (size_t i = 0; i < sizeof(s_dir_actions) / sizeof(s_dir_actions[0]); ++i) {
        if (s_dir_actions[i].code != code) continue;
        bool *held = &state->dir_held[s_dir_actions[i].dir];
        if (pressed && !*held) game_set_direction(state->game, s_dir_actions[i].dir);
        *held = pressed;
        return;
    }
}

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    host_state_t *state = value;
    if (!event) return;
    if (event->type == MOSAICO_HOST_INPUT_POINTER && event->track_id == 0) {
        game_set_pointer(state->game, event->x, event->y, event->pressed);
    } else if (event->type == MOSAICO_HOST_INPUT_CONTROL) {
        switch (event->code) {
        case MOSAICO_HOST_CONTROL_PAUSE: game_set_paused(state->game, true); break;
        case MOSAICO_HOST_CONTROL_RESUME: game_set_paused(state->game, false); break;
        case MOSAICO_HOST_CONTROL_RESET: game_reset(state->game); break;
        default: break;
        }
    } else if (event->type == MOSAICO_HOST_INPUT_ACTION) {
        direction_action(state, event->code, event->pressed);
        if (!event->pressed) return;
        if (event->code == GAME_ACTION_RESET) game_reset(state->game);
        if (event->code == GAME_ACTION_PAUSE)
            game_set_paused(state->game, !game_read(state->game).paused);
    }
}

static void update(void *value)
{
    game_update(((host_state_t *)value)->game);
}

static int render(void *value)
{
    return game_render(((host_state_t *)value)->game) ? 0 : -1;
}

static uint32_t state_hash(const void *value)
{
    const game_snapshot_t state = game_read(((const host_state_t *)value)->game);
    /* Hash named fields, avoiding platform-dependent struct padding. */
    uint32_t hash = state.tick;
    hash = hash * 31U + (uint32_t)state.pointer_x;
    hash = hash * 31U + (uint32_t)state.pointer_y;
    hash = hash * 31U + state.pointer_down;
    hash = hash * 31U + state.paused;
    hash = hash * 31U + (uint32_t)state.phase;
    hash = hash * 31U + state.won;
    hash = hash * 31U + (uint32_t)state.dir;
    hash = hash * 31U + state.score;
    hash = hash * 31U + state.best;
    hash = hash * 31U + (uint32_t)state.food_x;
    hash = hash * 31U + (uint32_t)state.food_y;
    return hash * 31U + state.body_hash;
}

static int state_json(const void *value, char *output, size_t capacity)
{
    if (!output || !capacity) return -1;
    const game_snapshot_t state = game_read(((const host_state_t *)value)->game);
    int size = snprintf(output, capacity,
        "{\"phase\":\"%s\",\"tick\":%lu,\"score\":%u,\"best\":%u,\"length\":%u,"
        "\"dir\":%d,\"head_x\":%d,\"head_y\":%d,\"food_x\":%d,\"food_y\":%d,"
        "\"pointer_x\":%ld,\"pointer_y\":%ld,\"pointer_down\":%s,"
        "\"body_hash\":\"%08lx\",\"state_hash\":\"%08lx\"}",
        phase_name(&state), (unsigned long)state.tick, (unsigned)state.score,
        (unsigned)state.best, (unsigned)state.length, (int)state.dir,
        (int)state.head_x, (int)state.head_y, (int)state.food_x, (int)state.food_y,
        (long)state.pointer_x, (long)state.pointer_y,
        state.pointer_down ? "true" : "false", (unsigned long)state.body_hash,
        (unsigned long)state_hash(value));
    return size < 0 || (size_t)size >= capacity ? -1 : size;
}

static const mosaico_game_module_v1_t s_module = {
    .descriptor = {MOSAICO_HOST_GAME_ABI_V1, GAME_NAME, GAME_NAME,
                   GAME_WIDTH, GAME_HEIGHT, GAME_TICK_HZ, 1},
    .state_size = sizeof(host_state_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render,
    .state_hash = state_hash, .state_json = state_json,
};

const mosaico_game_module_v1_t *mosaico_game_module_v1(void)
{
    return &s_module;
}
