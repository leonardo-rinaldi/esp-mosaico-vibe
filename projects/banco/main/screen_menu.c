// SPDX-License-Identifier: Apache-2.0
#include "screen_menu.h"
#include <stddef.h>
#include "screen_battery.h"
#include "ui_font.h"

enum {
    MENU_MARGIN = 12,
    MENU_GAP = 12,
    MENU_COLUMNS = 2,
    MENU_HEADER_HEIGHT = 84,
    MENU_TITLE_SCALE = 4,
    MENU_TITLE_Y = 12,
    MENU_SUBTITLE_SCALE = 2,
    MENU_SUBTITLE_Y = 56,
    TILE_NAME_SCALE = 2,
    TILE_NAME_Y = 13,               /* From the tile top. */
    TILE_NOTE_SCALE = 2,
    TILE_NOTE_Y = 38,
    TILE_BORDER = 2,
    DISPLAY_WIDTH = 480,
    DISPLAY_HEIGHT = 480,
    TILE_ROWS = (MENU_TILE_COUNT + MENU_COLUMNS - 1) / MENU_COLUMNS,
    TILE_WIDTH = (DISPLAY_WIDTH - 2 * MENU_MARGIN - (MENU_COLUMNS - 1) * MENU_GAP) / MENU_COLUMNS,
    TILE_HEIGHT = (DISPLAY_HEIGHT - MENU_HEADER_HEIGHT - MENU_MARGIN - (TILE_ROWS - 1) * MENU_GAP)
                  / TILE_ROWS,
};

_Static_assert((int)MENU_LINE_COUNT <= (int)UI_LINES_MAX, "every tile title and the battery need a view line");

typedef struct {
    text_id_t name;
    text_id_t note;
} tile_t;

static const tile_t s_tile[MENU_TILE_COUNT] = {
    {TXT_TILE_CONV_NAME, TXT_TILE_CONV_NOTE},
    {TXT_TILE_OHM_NAME, TXT_TILE_OHM_NOTE},
    {TXT_TILE_COLOR_NAME, TXT_TILE_COLOR_NOTE},
    {TXT_TILE_CABLE_NAME, TXT_TILE_CABLE_NOTE},
    {TXT_TILE_LEVEL_NAME, TXT_TILE_LEVEL_NOTE},
    {TXT_TILE_LED_NAME, TXT_TILE_LED_NOTE},
    {TXT_TILE_DIV_NAME, TXT_TILE_DIV_NOTE},
    {TXT_TILE_SMD_NAME, TXT_TILE_SMD_NOTE},
    {TXT_TILE_AWG_NAME, TXT_TILE_AWG_NOTE},
};

enum {
    BATTERY_HIT_X = 6, BATTERY_HIT_Y = 4, BATTERY_HIT_W = 128, BATTERY_HIT_H = 44,
    BATTERY_ICON_X = 14, BATTERY_ICON_Y = 15, BATTERY_ICON_W = 44, BATTERY_ICON_H = 22,
    BATTERY_TEXT_X = 66, BATTERY_TEXT_Y = 17, BATTERY_TEXT_SCALE = 2,
    LANG_BUTTON_WIDTH = 44, LANG_BUTTON_HEIGHT = 36, LANG_BUTTON_GAP = 8, LANG_BUTTON_TOP = 10,
};

static const char *const s_lang_label[LANG_COUNT] = {"IT", "EN"};

/* The language buttons sit in the header's top-right corner, EN last. */
static ui_rect_t language_rect(int lang)
{
    const int right = DISPLAY_WIDTH - MENU_MARGIN;
    const int x = right - (LANG_COUNT - lang) * LANG_BUTTON_WIDTH - (LANG_COUNT - 1 - lang) * LANG_BUTTON_GAP;
    return (ui_rect_t){(int16_t)x, LANG_BUTTON_TOP, LANG_BUTTON_WIDTH, LANG_BUTTON_HEIGHT};
}

static ui_rect_t tile_rect(int index)
{
    const int column = index % MENU_COLUMNS;
    const int row = index / MENU_COLUMNS;
    const bool wide = index == MENU_TILE_COUNT - 1 && MENU_TILE_COUNT % MENU_COLUMNS == 1;
    return (ui_rect_t){
        (int16_t)(MENU_MARGIN + (wide ? 0 : column * (TILE_WIDTH + MENU_GAP))),
        (int16_t)(MENU_HEADER_HEIGHT + row * (TILE_HEIGHT + MENU_GAP)),
        (int16_t)(wide ? DISPLAY_WIDTH - 2 * MENU_MARGIN : TILE_WIDTH), TILE_HEIGHT,
    };
}

void menu_relocalize(menu_state_t *state)
{
    for (int i = 0; i < MENU_TILE_COUNT; ++i)
        ui_lines_printf(&state->view, i, "%s", i18n_text(s_tile[i].name));
}

void menu_set_battery(menu_state_t *state, const battery_sample_t *sample)
{
    state->battery = *sample;
    char text[UI_LINE_MAX];
    battery_short_text(sample, text, sizeof(text));
    ui_lines_printf(&state->view, MENU_LINE_BATTERY, "%s", text);
}

bool menu_battery_hit(int32_t x, int32_t y)
{
    return ui_rect_contains((ui_rect_t){BATTERY_HIT_X, BATTERY_HIT_Y, BATTERY_HIT_W, BATTERY_HIT_H}, x, y);
}

void menu_init(menu_state_t *state)
{
    ui_lines_init(&state->view, MENU_LINE_COUNT);
    state->battery = (battery_sample_t){0};
    menu_relocalize(state);
    menu_set_battery(state, &state->battery);
}

int menu_language_hit(int32_t x, int32_t y)
{
    for (int lang = 0; lang < LANG_COUNT; ++lang) {
        if (ui_rect_contains(language_rect(lang), x, y)) return lang;
    }
    return -1;
}

int menu_hit(int32_t x, int32_t y)
{
    for (int i = 0; i < MENU_TILE_COUNT; ++i) {
        if (ui_rect_contains(tile_rect(i), x, y)) return i;
    }
    return -1;
}

static void draw_tile(int index, const ui_pointer_t *pointer)
{
    const ui_rect_t rect = tile_rect(index);
    const bool pressed = ui_pointer_inside(pointer, rect);
    ui_fill(rect, pressed ? UI_COLOR_KEY_DOWN : UI_COLOR_PANEL);
    ui_outline(rect, TILE_BORDER, pressed ? UI_COLOR_ACCENT : UI_COLOR_LINE);

    const int center = rect.x + rect.w / 2;
    const tile_t *tile = &s_tile[index];
    ui_text_draw_center(i18n_text(tile->name), center, rect.y + TILE_NAME_Y, TILE_NAME_SCALE,
                        pressed ? UI_COLOR_ON_ACCENT : UI_COLOR_ACCENT);
    ui_text_draw_center(i18n_text(tile->note), center, rect.y + TILE_NOTE_Y, TILE_NOTE_SCALE,
                        pressed ? UI_COLOR_ON_ACCENT : UI_COLOR_MUTED);
}

void menu_draw(const menu_state_t *state, const ui_pointer_t *pointer)
{
    const battery_sample_t *battery = &state->battery;
    const battery_state_t kind = battery_state(battery);
    const bool known = kind != BATTERY_UNKNOWN && kind != BATTERY_ABSENT;
    battery_draw_icon((ui_rect_t){BATTERY_ICON_X, BATTERY_ICON_Y, BATTERY_ICON_W, BATTERY_ICON_H},
                      known ? battery->percent : 0, battery_level_color(battery));
    ui_text_draw(ui_lines_get(&state->view, MENU_LINE_BATTERY), BATTERY_TEXT_X, BATTERY_TEXT_Y,
                 BATTERY_TEXT_SCALE, known ? battery_level_color(battery) : UI_COLOR_MUTED);
    ui_text_draw_center("BANCO", DISPLAY_WIDTH / 2, MENU_TITLE_Y, MENU_TITLE_SCALE, UI_COLOR_ACCENT);
    ui_text_draw_center(i18n_text(TXT_MENU_SUBTITLE), DISPLAY_WIDTH / 2, MENU_SUBTITLE_Y,
                        MENU_SUBTITLE_SCALE, UI_COLOR_MUTED);
    for (int lang = 0; lang < LANG_COUNT; ++lang) {
        ui_button(language_rect(lang), s_lang_label[lang], 2,
                  i18n_get() == (lang_t)lang ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    }
    for (int i = 0; i < MENU_TILE_COUNT; ++i) draw_tile(i, pointer);
}
