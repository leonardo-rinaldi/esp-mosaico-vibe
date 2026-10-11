// SPDX-License-Identifier: Apache-2.0
/* Bench assistant: four touch screens (converter, Ohm's law, resistor colour code,
 * cable voltage drop) behind a tab bar. The instance owns every screen's state and
 * is driven from a single loop, so no locking is needed. Host and device share it. */
#include <stdlib.h>
#include "game.h"
#include "game_config.h"
#include "mosaico_raylib_fast.h"
#include "screen_cable.h"
#include "screen_color.h"
#include "screen_conv.h"
#include "screen_ohm.h"
#include "ui_font.h"
#include "ui_widgets.h"

enum { TAB_LABEL_SCALE = 2 };

static const char *const s_tab_label[GAME_SCREEN_COUNT] = {"CONV", "OHM", "COLORI", "CAVO"};
static const char *const s_screen_name[GAME_SCREEN_COUNT] = {"conv", "ohm", "colori", "cavo"};

struct game_t {
    game_config_t config;
    uint32_t tick;
    ui_pointer_t pointer;
    bool paused;
    game_screen_t screen;
    conv_state_t conv;
    ohm_state_t ohm;
    color_state_t color;
    cable_state_t cable;
};

static void init_screens(game_handle_t game)
{
    conv_init(&game->conv);
    ohm_init(&game->ohm);
    color_init(&game->color);
    cable_init(&game->cable);
}

bool game_create(const game_config_t *config, game_handle_t *ret_handle)
{
    if (!ret_handle) return false;
    *ret_handle = NULL;
    if (!config || config->width != GAME_WIDTH || config->height != GAME_HEIGHT) return false;
    game_handle_t game = calloc(1, sizeof(*game));
    if (!game) return false;
    game->config = *config;
    init_screens(game);
    *ret_handle = game;
    return true;
}

void game_delete(game_handle_t game)
{
    free(game);
}

void game_reset(game_handle_t game)
{
    if (!game) return;
    game->screen = GAME_SCREEN_CONVERTER;
    game->paused = false;
    init_screens(game);
}

void game_set_paused(game_handle_t game, bool paused)
{
    if (game) game->paused = paused;
}

static const ui_lines_t *active_view(const struct game_t *game)
{
    switch (game->screen) {
    case GAME_SCREEN_CONVERTER: return &game->conv.view;
    case GAME_SCREEN_OHM: return &game->ohm.view;
    case GAME_SCREEN_COLORS: return &game->color.view;
    case GAME_SCREEN_CABLE: return &game->cable.view;
    default: return &game->conv.view;
    }
}

static void tap(game_handle_t game, int32_t x, int32_t y)
{
    if (y < UI_TAB_HEIGHT) {
        const int tab = (int)(x / (game->config.width / GAME_SCREEN_COUNT));
        if (tab >= 0 && tab < GAME_SCREEN_COUNT) game->screen = (game_screen_t)tab;
        return;
    }
    switch (game->screen) {
    case GAME_SCREEN_CONVERTER: conv_tap(&game->conv, x, y); break;
    case GAME_SCREEN_OHM: ohm_tap(&game->ohm, x, y); break;
    case GAME_SCREEN_COLORS: color_tap(&game->color, x, y); break;
    case GAME_SCREEN_CABLE: cable_tap(&game->cable, x, y); break;
    default: break;
    }
}

void game_set_pointer(game_handle_t game, int32_t x, int32_t y, bool pressed)
{
    if (!game) return;
    const bool was_down = game->pointer.down;
    game->pointer.x = x < 0 ? 0 : (x >= game->config.width ? game->config.width - 1 : x);
    game->pointer.y = y < 0 ? 0 : (y >= game->config.height ? game->config.height - 1 : y);
    game->pointer.down = pressed;
    if (pressed && !was_down && !game->paused) tap(game, game->pointer.x, game->pointer.y);
}

void game_update(game_handle_t game)
{
    if (!game || game->paused) return;
    ++game->tick;
}

static void draw_tabs(const struct game_t *game)
{
    const int width = game->config.width / GAME_SCREEN_COUNT;
    for (int i = 0; i < GAME_SCREEN_COUNT; ++i) {
        const ui_rect_t rect = {(int16_t)(i * width), 0, (int16_t)width, UI_TAB_HEIGHT};
        ui_button(rect, s_tab_label[i], TAB_LABEL_SCALE,
                  (int)game->screen == i ? UI_BUTTON_SELECTED : UI_BUTTON_DISABLED, &game->pointer);
    }
}

bool game_render(game_handle_t game)
{
    if (!game) return false;
    BeginDrawing();
    if (!MosaicoFastFrameAvailable()) {
        EndDrawing();
        return false;
    }
    ClearBackground(UI_COLOR_BG);
    draw_tabs(game);
    switch (game->screen) {
    case GAME_SCREEN_CONVERTER: conv_draw(&game->conv, &game->pointer, game->tick); break;
    case GAME_SCREEN_OHM: ohm_draw(&game->ohm, &game->pointer); break;
    case GAME_SCREEN_COLORS: color_draw(&game->color, &game->pointer); break;
    case GAME_SCREEN_CABLE: cable_draw(&game->cable, &game->pointer); break;
    default: break;
    }
    EndDrawing();
    return true;
}

int game_line_count(game_handle_t game)
{
    return game ? active_view(game)->count : 0;
}

const char *game_line(game_handle_t game, int index)
{
    return game ? ui_lines_get(active_view(game), index) : NULL;
}

const char *game_screen_name(game_screen_t screen)
{
    return (unsigned)screen < GAME_SCREEN_COUNT ? s_screen_name[screen] : "";
}

game_snapshot_t game_read(game_handle_t game)
{
    if (!game) return (game_snapshot_t){0};
    uint32_t hash = 2166136261U;    /* FNV-1a over the text of the active screen. */
    const ui_lines_t *view = active_view(game);
    for (int line = 0; line < view->count; ++line) {
        for (const char *p = view->text[line]; *p; ++p) hash = (hash ^ (uint8_t)*p) * 16777619U;
        hash = (hash ^ 0xFFU) * 16777619U;
    }
    return (game_snapshot_t){
        .tick = game->tick,
        .pointer_x = game->pointer.x,
        .pointer_y = game->pointer.y,
        .pointer_down = game->pointer.down,
        .paused = game->paused,
        .screen = game->screen,
        .view_hash = hash,
    };
}
