// SPDX-License-Identifier: Apache-2.0
/* Bench assistant: a home menu that opens four touch screens (converter, Ohm's law,
 * resistor colour code, cable voltage drop). Each function has a back bar to return
 * to the menu. The instance owns every screen's state and is driven from a single
 * loop, so no locking is needed. Host and device share it. */
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "game_config.h"
#include "i18n.h"
#include "mosaico_raylib_fast.h"
#include "screen_battery.h"
#include "screen_cable.h"
#include "screen_color.h"
#include "screen_conv.h"
#include "screen_awg.h"
#include "screen_divider.h"
#include "screen_led.h"
#include "screen_level.h"
#include "screen_power.h"
#include "screen_smd.h"
#include "screen_menu.h"
#include "screen_ohm.h"
#include "ui_font.h"
#include "ui_widgets.h"

enum {
    HEADER_TEXT_SCALE = 2,
    BACK_BUTTON_WIDTH = 150,        /* The rest of the header only shows the title. */
    POWER_BUTTON_X = 330,           /* Battery screen: the button to the power-saving screen. */
    DIM_PERCENT = 10,               /* Brightness of a dimmed screen. */
    DARK_FRAMES = 4,                /* More than the engine's three frame buffers. */
};

static const char *const s_screen_name[GAME_SCREEN_COUNT] = {
    "menu", "conv", "ohm", "colori", "cavo", "livella", "led", "partitore", "smd", "awg", "batteria",
    "risparmio",
};
/* Header title of each function screen; the menu has no header. */
static const text_id_t s_screen_title[GAME_SCREEN_COUNT] = {
    [GAME_SCREEN_CONVERTER] = TXT_TITLE_CONV, [GAME_SCREEN_OHM] = TXT_TITLE_OHM,
    [GAME_SCREEN_COLORS] = TXT_TITLE_COLOR, [GAME_SCREEN_CABLE] = TXT_TITLE_CABLE,
    [GAME_SCREEN_LEVEL] = TXT_TITLE_LEVEL, [GAME_SCREEN_LED] = TXT_TITLE_LED,
    [GAME_SCREEN_DIVIDER] = TXT_TITLE_DIV, [GAME_SCREEN_SMD] = TXT_TITLE_SMD, [GAME_SCREEN_AWG] = TXT_TITLE_AWG,
    [GAME_SCREEN_BATTERY] = TXT_TITLE_BATTERY, [GAME_SCREEN_POWER] = TXT_TITLE_POWER,
};

struct game_t {
    game_config_t config;
    uint32_t tick;
    ui_pointer_t pointer;
    bool paused;
    game_screen_t screen;
    menu_state_t menu;
    conv_state_t conv;
    ohm_state_t ohm;
    color_state_t color;
    cable_state_t cable;
    level_state_t level;
    led_state_t led;
    divider_state_t divider;
    smd_state_t smd;
    awg_state_t awg;
    battery_screen_t battery;
    power_state_t power;
    uint32_t idle_ticks;            /* Ticks since the last touch; drives dimming and screen off. */
    uint8_t dark_frames;            /* Black frames drawn since the screen turned off. */
    settings_t seen;                /* Last values reported by game_settings_poll. */
};

/* Texts built at input time are rewritten; entered values are kept. */
static void relocalize_screens(game_handle_t game)
{
    menu_relocalize(&game->menu);
    ohm_relocalize(&game->ohm);
    color_relocalize(&game->color);
    cable_relocalize(&game->cable);
    level_relocalize(&game->level);
    led_relocalize(&game->led);
    divider_relocalize(&game->divider);
    smd_relocalize(&game->smd);
    awg_relocalize(&game->awg);
    power_relocalize(&game->power);
    battery_screen_relocalize(&game->battery);
}

static void init_screens(game_handle_t game)
{
    const battery_sample_t last_battery = game->battery.sample;
    const power_state_t last_power = game->power;
    menu_init(&game->menu);
    conv_init(&game->conv);
    ohm_init(&game->ohm);
    color_init(&game->color);
    cable_init(&game->cable);
    level_init(&game->level);
    led_init(&game->led);
    divider_init(&game->divider);
    smd_init(&game->smd);
    awg_init(&game->awg);
    battery_screen_init(&game->battery);
    power_init(&game->power);
    game->idle_ticks = 0;
    /* The gauge keeps reporting on its own, so a reset must not forget the last reading. */
    if (last_battery.valid) battery_screen_set(&game->battery, &last_battery);
    menu_set_battery(&game->menu, &game->battery.sample);
    /* The power choices are settings, not entries: a reset keeps them. */
    if (last_power.view.count) {
        game->power = last_power;
        power_relocalize(&game->power);
    }
}

bool game_create(const game_config_t *config, game_handle_t *ret_handle)
{
    if (!ret_handle) return false;
    *ret_handle = NULL;
    if (!config || config->width != GAME_WIDTH || config->height != GAME_HEIGHT) return false;
    game_handle_t game = calloc(1, sizeof(*game));
    if (!game) return false;
    game->config = *config;
    i18n_set(LANG_IT);
    init_screens(game);
    game_settings_get(game, &game->seen);
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
    game->screen = GAME_SCREEN_MENU;
    game->paused = false;
    i18n_set(LANG_IT);
    init_screens(game);
}

static void copy_entry(char *to, const calc_entry_t *entry)
{
    memset(to, 0, SETTINGS_ENTRY_SIZE);
    memcpy(to, entry->text, SETTINGS_ENTRY_SIZE - 1);
}

void game_settings_get(game_handle_t game, settings_t *out)
{
    if (!game || !out) return;
    memset(out, 0, sizeof(*out));
    out->language = (uint8_t)i18n_get();
    out->conv_qty = (uint8_t)game->conv.qty;
    out->conv_unit = (uint8_t)game->conv.unit;
    copy_entry(out->conv_entry, &game->conv.entry);
    for (int i = 0; i < SETTINGS_OHM_ROWS; ++i) {
        copy_entry(out->ohm_entry[i], &game->ohm.entry[i]);
        out->ohm_unit[i] = (uint8_t)game->ohm.unit[i];
        out->ohm_unit_auto[i] = game->ohm.unit_auto[i];
    }
    out->ohm_selected = (uint8_t)game->ohm.selected;
    for (int i = 0; i < SETTINGS_CABLE_FIELDS; ++i) copy_entry(out->cable_entry[i], &game->cable.entry[i]);
    out->cable_material = (uint8_t)game->cable.material;
    out->cable_system = (uint8_t)game->cable.system;
    out->cable_selected = (uint8_t)game->cable.selected;
    out->color_bands = (uint8_t)game->color.bands;
    out->color_selected = (uint8_t)game->color.selected;
    for (int i = 0; i < RESISTOR_MAX_BANDS; ++i) out->color_color[i] = (uint8_t)game->color.color[i];
    out->level_mode = (uint8_t)game->level.mode;
    memcpy(out->level_zero, game->level.zero, sizeof(out->level_zero));
    out->power_brightness = game->power.brightness;
    out->power_sleep = game->power.sleep;
}

/* Rebuild an entry key by key, so only text that is a valid number gets in. Text
 * with anything else in it is ignored whole and the entry keeps its value. */
static void restore_entry(calc_entry_t *entry, const char *text)
{
    calc_entry_t rebuilt;
    calc_entry_clear(&rebuilt);
    for (int i = 0; i < SETTINGS_ENTRY_SIZE - 1 && text[i]; ++i) {
        const char c = text[i];
        bool accepted = false;
        if (c >= '0' && c <= '9') accepted = calc_entry_digit(&rebuilt, c);
        else if (c == ',') accepted = calc_entry_point(&rebuilt);
        else if (c == '-' && i == 0) accepted = calc_entry_toggle_sign(&rebuilt);
        if (!accepted) return;
    }
    *entry = rebuilt;
}

void game_settings_apply(game_handle_t game, const settings_t *in)
{
    if (!game || !in) return;
    settings_t value = *in;
    /* Entry text came from storage: make sure it ends before it is walked. */
    value.conv_entry[SETTINGS_ENTRY_SIZE - 1] = '\0';
    for (int i = 0; i < SETTINGS_OHM_ROWS; ++i) value.ohm_entry[i][SETTINGS_ENTRY_SIZE - 1] = '\0';
    for (int i = 0; i < SETTINGS_CABLE_FIELDS; ++i) value.cable_entry[i][SETTINGS_ENTRY_SIZE - 1] = '\0';

    if (value.language < LANG_COUNT) i18n_set((lang_t)value.language);

    if (value.conv_qty < CALC_QTY_COUNT && value.conv_unit < CALC_UNITS_PER_QTY) {
        game->conv.qty = (calc_qty_t)value.conv_qty;
        game->conv.unit = value.conv_unit;
        restore_entry(&game->conv.entry, value.conv_entry);
    }
    for (int i = 0; i < SETTINGS_OHM_ROWS; ++i) {
        restore_entry(&game->ohm.entry[i], value.ohm_entry[i]);
        if (value.ohm_unit[i] < CALC_UNITS_PER_QTY) {
            game->ohm.unit[i] = value.ohm_unit[i];
            game->ohm.unit_auto[i] = value.ohm_unit_auto[i] != 0;
        }
    }
    if (value.ohm_selected < SETTINGS_OHM_ROWS) game->ohm.selected = value.ohm_selected;

    for (int i = 0; i < SETTINGS_CABLE_FIELDS; ++i) restore_entry(&game->cable.entry[i], value.cable_entry[i]);
    if (value.cable_material <= CABLE_ALUMINIUM) game->cable.material = (cable_material_t)value.cable_material;
    if (value.cable_system <= CABLE_THREE_PHASE) game->cable.system = (cable_system_t)value.cable_system;
    if (value.cable_selected < SETTINGS_CABLE_FIELDS) game->cable.selected = value.cable_selected;

    if (value.color_bands == RESISTOR_MIN_BANDS || value.color_bands == RESISTOR_MAX_BANDS) {
        bool valid = value.color_selected < value.color_bands;
        for (int band = 0; valid && band < value.color_bands; ++band) {
            valid = value.color_color[band] < COLOR_COUNT &&
                    calc_band_allows(calc_band_kind(value.color_bands, band), (calc_color_t)value.color_color[band]);
        }
        if (valid) {
            game->color.bands = value.color_bands;
            game->color.selected = value.color_selected;
            for (int band = 0; band < value.color_bands; ++band)
                game->color.color[band] = (calc_color_t)value.color_color[band];
        }
    }

    if (value.level_mode < LEVEL_MODE_COUNT) game->level.mode = (level_mode_t)value.level_mode;
    for (int mode = 0; mode < LEVEL_MODE_COUNT; ++mode) {
        for (int axis = 0; axis < LEVEL_AXES; ++axis) {
            const int16_t zero = value.level_zero[mode][axis];
            if (zero >= -LEVEL_ZERO_LIMIT && zero <= LEVEL_ZERO_LIMIT) game->level.zero[mode][axis] = zero;
        }
    }
    game->level.valid = false;

    if (value.power_brightness < POWER_BRIGHTNESS_COUNT) game->power.brightness = value.power_brightness;
    if (value.power_sleep < POWER_SLEEP_COUNT) game->power.sleep = value.power_sleep;
    power_relocalize(&game->power);

    conv_relocalize(&game->conv);
    relocalize_screens(game);
    game_settings_get(game, &game->seen);      /* Restoring is not a change to save. */
}

bool game_settings_poll(game_handle_t game, settings_t *out)
{
    if (!game || !out) return false;
    settings_t now;
    game_settings_get(game, &now);
    if (memcmp(&now, &game->seen, sizeof(now)) == 0) return false;
    game->seen = now;
    *out = now;
    return true;
}

void game_set_battery(game_handle_t game, const battery_sample_t *sample)
{
    if (!game || !sample) return;
    battery_screen_set(&game->battery, sample);
    menu_set_battery(&game->menu, &game->battery.sample);
}

void game_set_paused(game_handle_t game, bool paused)
{
    if (game) game->paused = paused;
}

static const ui_lines_t *active_view(const struct game_t *game)
{
    switch (game->screen) {
    case GAME_SCREEN_MENU: return &game->menu.view;
    case GAME_SCREEN_CONVERTER: return &game->conv.view;
    case GAME_SCREEN_OHM: return &game->ohm.view;
    case GAME_SCREEN_COLORS: return &game->color.view;
    case GAME_SCREEN_CABLE: return &game->cable.view;
    case GAME_SCREEN_LEVEL: return &game->level.view;
    case GAME_SCREEN_LED: return &game->led.view;
    case GAME_SCREEN_DIVIDER: return &game->divider.view;
    case GAME_SCREEN_SMD: return &game->smd.view;
    case GAME_SCREEN_AWG: return &game->awg.view;
    case GAME_SCREEN_POWER: return &game->power.view;
    case GAME_SCREEN_BATTERY: return &game->battery.view;
    default: return &game->menu.view;
    }
}

static void tap(game_handle_t game, int32_t x, int32_t y)
{
    if (game->screen == GAME_SCREEN_MENU) {
        const int lang = menu_language_hit(x, y);
        if (lang >= 0) {
            i18n_set((lang_t)lang);
            relocalize_screens(game);
            return;
        }
        if (menu_battery_hit(x, y)) {
            game->screen = GAME_SCREEN_BATTERY;
            return;
        }
        const int tile = menu_hit(x, y);
        /* The tiles are in the same order as the function screens after the menu. */
        if (tile >= 0) game->screen = (game_screen_t)(GAME_SCREEN_CONVERTER + tile);
        return;
    }
    if (y < UI_TAB_HEIGHT) {
        if (x < BACK_BUTTON_WIDTH)
            game->screen = game->screen == GAME_SCREEN_POWER ? GAME_SCREEN_BATTERY : GAME_SCREEN_MENU;
        else if (game->screen == GAME_SCREEN_BATTERY && x >= POWER_BUTTON_X)
            game->screen = GAME_SCREEN_POWER;
        return;
    }
    switch (game->screen) {
    case GAME_SCREEN_CONVERTER: conv_tap(&game->conv, x, y); break;
    case GAME_SCREEN_OHM: ohm_tap(&game->ohm, x, y); break;
    case GAME_SCREEN_COLORS: color_tap(&game->color, x, y); break;
    case GAME_SCREEN_CABLE: cable_tap(&game->cable, x, y); break;
    case GAME_SCREEN_LEVEL: level_tap(&game->level, x, y); break;
    case GAME_SCREEN_LED: led_tap(&game->led, x, y); break;
    case GAME_SCREEN_DIVIDER: divider_tap(&game->divider, x, y); break;
    case GAME_SCREEN_SMD: smd_tap(&game->smd, x, y); break;
    case GAME_SCREEN_AWG: awg_tap(&game->awg, x, y); break;
    case GAME_SCREEN_POWER: power_tap(&game->power, x, y); break;
    default: break;
    }
}

int game_display_brightness(game_handle_t game)
{
    if (!game) return 0;
    const int normal = power_brightness_percent(&game->power);
    const uint32_t limit = (uint32_t)power_sleep_seconds(&game->power) * GAME_TICK_HZ;
    if (limit == 0) return normal;
    if (game->idle_ticks >= limit) return 0;
    if (game->idle_ticks >= limit / 2) return normal < DIM_PERCENT ? normal : DIM_PERCENT;
    return normal;
}

void game_set_pointer(game_handle_t game, int32_t x, int32_t y, bool pressed)
{
    if (!game) return;
    /* The touch that wakes a dark screen only wakes it: nobody sees what it would press. */
    const bool screen_was_off = game_display_brightness(game) == 0;
    game->idle_ticks = 0;
    const bool was_down = game->pointer.down;
    game->pointer.x = x < 0 ? 0 : (x >= game->config.width ? game->config.width - 1 : x);
    game->pointer.y = y < 0 ? 0 : (y >= game->config.height ? game->config.height - 1 : y);
    game->pointer.down = pressed;
    if (pressed && !was_down && !game->paused && !screen_was_off) tap(game, game->pointer.x, game->pointer.y);
}

void game_update(game_handle_t game)
{
    if (!game || game->paused) return;
    ++game->tick;
    if (game->idle_ticks < UINT32_MAX) ++game->idle_ticks;
    /* The level only listens while it is on screen; the other screens ignore the sensor. */
    if (game->screen == GAME_SCREEN_LEVEL) {
        const Vector3 g = MosaicoFastGetImuAcceleration();
        level_update(&game->level, g.x, g.y, g.z);
    }
}

/* Back bar shown above every function: a back button and the function's title. */
static void draw_header(const struct game_t *game)
{
    const ui_rect_t bar = {0, 0, (int16_t)game->config.width, UI_TAB_HEIGHT};
    ui_fill(bar, UI_COLOR_PANEL);
    const ui_rect_t back = {0, 0, BACK_BUTTON_WIDTH, UI_TAB_HEIGHT};
    ui_button(back, "< MENU", HEADER_TEXT_SCALE, UI_BUTTON_FUNCTION, &game->pointer);
    int title_right = game->config.width;
    if (game->screen == GAME_SCREEN_BATTERY) {
        const ui_rect_t saving = {POWER_BUTTON_X, 0, (int16_t)(game->config.width - POWER_BUTTON_X), UI_TAB_HEIGHT};
        ui_button(saving, i18n_text(TXT_POWER_BUTTON), HEADER_TEXT_SCALE, UI_BUTTON_FUNCTION, &game->pointer);
        title_right = POWER_BUTTON_X;
    }
    const int title_center = BACK_BUTTON_WIDTH + (title_right - BACK_BUTTON_WIDTH) / 2;
    ui_text_draw_center(i18n_text(s_screen_title[game->screen]), title_center,
                        (UI_TAB_HEIGHT - ui_text_height(HEADER_TEXT_SCALE)) / 2,
                        HEADER_TEXT_SCALE, UI_COLOR_ACCENT);
}

bool game_render(game_handle_t game)
{
    if (!game) return false;
    /* Everything is drawn through the brightness, so 0 draws black. Once every frame buffer
     * shows black the screen is left alone, which is what saves the battery. */
    const int brightness = game_display_brightness(game);
    if (brightness == 0) {
        if (game->dark_frames >= DARK_FRAMES) return true;
        ++game->dark_frames;
    } else {
        game->dark_frames = 0;
    }
    ui_dim_set(brightness);
    BeginDrawing();
    if (!MosaicoFastFrameAvailable()) {
        EndDrawing();
        return false;
    }
    ClearBackground(UI_COLOR_BG);
    if (game->screen != GAME_SCREEN_MENU) draw_header(game);
    switch (game->screen) {
    case GAME_SCREEN_MENU: menu_draw(&game->menu, &game->pointer); break;
    case GAME_SCREEN_CONVERTER: conv_draw(&game->conv, &game->pointer, game->tick); break;
    case GAME_SCREEN_OHM: ohm_draw(&game->ohm, &game->pointer); break;
    case GAME_SCREEN_COLORS: color_draw(&game->color, &game->pointer); break;
    case GAME_SCREEN_CABLE: cable_draw(&game->cable, &game->pointer); break;
    case GAME_SCREEN_LEVEL: level_draw(&game->level, &game->pointer); break;
    case GAME_SCREEN_LED: led_draw(&game->led, &game->pointer); break;
    case GAME_SCREEN_DIVIDER: divider_draw(&game->divider, &game->pointer); break;
    case GAME_SCREEN_SMD: smd_draw(&game->smd, &game->pointer); break;
    case GAME_SCREEN_AWG: awg_draw(&game->awg, &game->pointer); break;
    case GAME_SCREEN_POWER: power_draw(&game->power, &game->pointer); break;
    case GAME_SCREEN_BATTERY: battery_screen_draw(&game->battery); break;
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
