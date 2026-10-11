// SPDX-License-Identifier: Apache-2.0
#include "screen_menu.h"
#include <stddef.h>
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
    TILE_ICON_SCALE = 5,
    TILE_ICON_Y = 26,               /* From the tile top. */
    TILE_NAME_SCALE = 2,
    TILE_NAME_Y = 100,
    TILE_NOTE_SCALE = 2,
    TILE_NOTE_Y = 132,
    TILE_BORDER = 2,
    DISPLAY_WIDTH = 480,
    DISPLAY_HEIGHT = 480,
    TILE_ROWS = MENU_TILE_COUNT / MENU_COLUMNS,
    TILE_WIDTH = (DISPLAY_WIDTH - 2 * MENU_MARGIN - (MENU_COLUMNS - 1) * MENU_GAP) / MENU_COLUMNS,
    TILE_HEIGHT = (DISPLAY_HEIGHT - MENU_HEADER_HEIGHT - MENU_MARGIN - (TILE_ROWS - 1) * MENU_GAP)
                  / TILE_ROWS,
};

_Static_assert(MENU_TILE_COUNT % MENU_COLUMNS == 0, "tiles must fill whole rows");
_Static_assert((int)MENU_TILE_COUNT <= (int)UI_LINES_MAX, "every tile title needs a view line");

typedef struct {
    const char *icon;               /* Large text; NULL when the tile draws its own icon. */
    text_id_t name;
    text_id_t note;
} tile_t;

static const tile_t s_tile[MENU_TILE_COUNT] = {
    {"kΩ", TXT_TILE_CONV_NAME, TXT_TILE_CONV_NOTE},
    {"V I R", TXT_TILE_OHM_NAME, TXT_TILE_OHM_NOTE},
    {NULL, TXT_TILE_COLOR_NAME, TXT_TILE_COLOR_NOTE},
    {"mm²", TXT_TILE_CABLE_NAME, TXT_TILE_CABLE_NOTE},
};

static const Color COLOR_RESISTOR = {210, 170, 120, 255};
static const Color COLOR_LEAD = {150, 155, 165, 255};
static const Color COLOR_BAND_FIRST = {250, 220, 40, 255};
static const Color COLOR_BAND_SECOND = {150, 70, 200, 255};
static const Color COLOR_BAND_THIRD = {225, 40, 50, 255};
static const Color COLOR_BAND_TOLERANCE = {210, 170, 40, 255};

enum {
    RESISTOR_WIDTH = 104,
    RESISTOR_HEIGHT = 34,
    RESISTOR_LEAD = 22,
    RESISTOR_BAND_WIDTH = 8,
    RESISTOR_BAND_X = 18,
    RESISTOR_BAND_PITCH = 20,
    RESISTOR_TOLERANCE_X = 84,
    RESISTOR_LEAD_THICKNESS = 4,
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
    return (ui_rect_t){
        (int16_t)(MENU_MARGIN + column * (TILE_WIDTH + MENU_GAP)),
        (int16_t)(MENU_HEADER_HEIGHT + row * (TILE_HEIGHT + MENU_GAP)),
        TILE_WIDTH, TILE_HEIGHT,
    };
}

void menu_relocalize(menu_state_t *state)
{
    for (int i = 0; i < MENU_TILE_COUNT; ++i)
        ui_lines_printf(&state->view, i, "%s", i18n_text(s_tile[i].name));
}

void menu_init(menu_state_t *state)
{
    ui_lines_init(&state->view, MENU_TILE_COUNT);
    menu_relocalize(state);
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

static void draw_resistor_icon(ui_rect_t tile, int top)
{
    const int total = RESISTOR_WIDTH + 2 * RESISTOR_LEAD;
    const int left = tile.x + (tile.w - total) / 2;
    const int body_x = left + RESISTOR_LEAD;
    const int body_y = top + (UI_FONT_HEIGHT * TILE_ICON_SCALE - RESISTOR_HEIGHT) / 2;
    const int mid_y = body_y + RESISTOR_HEIGHT / 2;
    DrawRectangle(left, mid_y - RESISTOR_LEAD_THICKNESS / 2, total, RESISTOR_LEAD_THICKNESS,
                  COLOR_LEAD);
    DrawRectangle(body_x, body_y, RESISTOR_WIDTH, RESISTOR_HEIGHT, COLOR_RESISTOR);
    const Color bands[] = {COLOR_BAND_FIRST, COLOR_BAND_SECOND, COLOR_BAND_THIRD};
    for (int i = 0; i < 3; ++i) {
        DrawRectangle(body_x + RESISTOR_BAND_X + i * RESISTOR_BAND_PITCH, body_y,
                      RESISTOR_BAND_WIDTH, RESISTOR_HEIGHT, bands[i]);
    }
    DrawRectangle(body_x + RESISTOR_TOLERANCE_X, body_y, RESISTOR_BAND_WIDTH, RESISTOR_HEIGHT,
                  COLOR_BAND_TOLERANCE);
}

static void draw_tile(int index, const ui_pointer_t *pointer)
{
    const ui_rect_t rect = tile_rect(index);
    const bool pressed = ui_pointer_inside(pointer, rect);
    ui_fill(rect, pressed ? UI_COLOR_KEY_DOWN : UI_COLOR_PANEL);
    ui_outline(rect, TILE_BORDER, pressed ? UI_COLOR_ACCENT : UI_COLOR_LINE);

    const int center = rect.x + rect.w / 2;
    const tile_t *tile = &s_tile[index];
    if (tile->icon) {
        ui_text_draw_center(tile->icon, center, rect.y + TILE_ICON_Y, TILE_ICON_SCALE,
                            pressed ? UI_COLOR_ON_ACCENT : UI_COLOR_ACCENT);
    } else {
        draw_resistor_icon(rect, rect.y + TILE_ICON_Y);
    }
    ui_text_draw_center(i18n_text(tile->name), center, rect.y + TILE_NAME_Y, TILE_NAME_SCALE,
                        pressed ? UI_COLOR_ON_ACCENT : UI_COLOR_TEXT);
    ui_text_draw_center(i18n_text(tile->note), center, rect.y + TILE_NOTE_Y, TILE_NOTE_SCALE,
                        pressed ? UI_COLOR_ON_ACCENT : UI_COLOR_MUTED);
}

void menu_draw(const menu_state_t *state, const ui_pointer_t *pointer)
{
    (void)state;
    ui_text_draw_center("BANCO", DISPLAY_WIDTH / 2, MENU_TITLE_Y, MENU_TITLE_SCALE, UI_COLOR_ACCENT);
    ui_text_draw_center(i18n_text(TXT_MENU_SUBTITLE), DISPLAY_WIDTH / 2, MENU_SUBTITLE_Y,
                        MENU_SUBTITLE_SCALE, UI_COLOR_MUTED);
    for (int lang = 0; lang < LANG_COUNT; ++lang) {
        ui_button(language_rect(lang), s_lang_label[lang], 2,
                  i18n_get() == (lang_t)lang ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    }
    for (int i = 0; i < MENU_TILE_COUNT; ++i) draw_tile(i, pointer);
}
