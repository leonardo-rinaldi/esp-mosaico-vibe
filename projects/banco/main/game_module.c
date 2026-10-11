// SPDX-License-Identifier: Apache-2.0
#include <stdarg.h>
#include <stdio.h>
#include "game.h"
#include "game_config.h"
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"

typedef struct {
    game_handle_t game;
} host_state_t;

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
    } else if (event->type == MOSAICO_HOST_INPUT_ACTION && event->pressed) {
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
    hash = hash * 31U + (uint32_t)state.screen;
    return hash * 31U + state.view_hash;
}

typedef struct {
    char *output;
    size_t capacity;
    size_t length;
    bool overflow;
} json_t;

static void json_add(json_t *json, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

static void json_add(json_t *json, const char *format, ...)
{
    if (json->overflow) return;
    va_list args;
    va_start(args, format);
    const int size = vsnprintf(json->output + json->length, json->capacity - json->length, format, args);
    va_end(args);
    if (size < 0 || (size_t)size >= json->capacity - json->length) json->overflow = true;
    else json->length += (size_t)size;
}

/* The lines hold UI text only: capitals, digits, symbols, no quotes or backslashes. */
static int state_json(const void *value, char *output, size_t capacity)
{
    if (!output || !capacity) return -1;
    game_handle_t game = ((const host_state_t *)value)->game;
    const game_snapshot_t state = game_read(game);
    json_t json = {output, capacity, 0, false};
    json_add(&json,
        "{\"phase\":\"%s\",\"screen\":\"%s\",\"tick\":%lu,\"pointer_x\":%ld,\"pointer_y\":%ld,"
        "\"pointer_down\":%s,\"lines\":[",
        state.paused ? "paused" : "running", game_screen_name(state.screen),
        (unsigned long)state.tick, (long)state.pointer_x, (long)state.pointer_y,
        state.pointer_down ? "true" : "false");
    for (int i = 0; i < game_line_count(game); ++i)
        json_add(&json, "%s\"%s\"", i ? "," : "", game_line(game, i));
    json_add(&json, "],\"state_hash\":\"%08lx\"}", (unsigned long)state_hash(value));
    return json.overflow ? -1 : (int)json.length;
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
