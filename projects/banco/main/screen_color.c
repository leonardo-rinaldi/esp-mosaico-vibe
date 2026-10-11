// SPDX-License-Identifier: Apache-2.0
#include "screen_color.h"
#include <stdio.h>
#include <stdlib.h>
#include "ui_font.h"

enum {
    TOGGLE_TOP = 48, TOGGLE_HEIGHT = 36, TOGGLE_WIDTH = 240,
    BODY_X = 70, BODY_Y = 106, BODY_WIDTH = 340, BODY_HEIGHT = 60, END_RADIUS = 30,
    LEAD_Y = 134, LEAD_THICKNESS = 4, LEAD_WIDTH = 60,
    BAND_WIDTH = 26, BAND_HIT_MARGIN = 8, BAND_TOP = BODY_Y, BAND_HEIGHT = BODY_HEIGHT,
    VALUE_Y = 182, VALUE_SCALE = 5, INFO_Y = 232, INFO_SCALE = 2,
    PALETTE_COLUMNS = 6, PALETTE_PITCH_X = 78, PALETTE_FIRST_X = 44,
    PALETTE_FIRST_Y = 296, PALETTE_PITCH_Y = 84, PALETTE_RADIUS = 26,
    PALETTE_CELL_HALF_W = 39, PALETTE_CELL_HALF_H = 40, LABEL_OFFSET = 34,
    FOOTER_Y = 452,
};

static const Color s_color[COLOR_COUNT] = {
    [COLOR_BLACK] = {10, 10, 10, 255},   [COLOR_BROWN] = {120, 70, 30, 255},
    [COLOR_RED] = {220, 40, 40, 255},    [COLOR_ORANGE] = {250, 130, 20, 255},
    [COLOR_YELLOW] = {245, 215, 40, 255}, [COLOR_GREEN] = {40, 170, 70, 255},
    [COLOR_BLUE] = {40, 80, 220, 255},   [COLOR_VIOLET] = {150, 60, 200, 255},
    [COLOR_GREY] = {140, 140, 150, 255}, [COLOR_WHITE] = {245, 245, 245, 255},
    [COLOR_GOLD] = {212, 175, 55, 255},  [COLOR_SILVER] = {190, 190, 200, 255},
};
static const char *const s_color_name[COLOR_COUNT] = {
    [COLOR_BLACK] = "NERO", [COLOR_BROWN] = "MARR", [COLOR_RED] = "ROSSO",
    [COLOR_ORANGE] = "ARAN", [COLOR_YELLOW] = "GIALLO", [COLOR_GREEN] = "VERDE",
    [COLOR_BLUE] = "BLU", [COLOR_VIOLET] = "VIOLA", [COLOR_GREY] = "GRIGIO",
    [COLOR_WHITE] = "BIANCO", [COLOR_GOLD] = "ORO", [COLOR_SILVER] = "ARG",
};
static const char *const s_kind_name[] = {
    [BAND_DIGIT] = "CIFRA", [BAND_MULTIPLIER] = "MOLTIPLICATORE", [BAND_TOLERANCE] = "TOLLERANZA",
};

/* Left edge of each band on the resistor drawing, per band count. */
static const int16_t s_band_x4[4] = {110, 165, 220, 350};
static const int16_t s_band_x5[5] = {95, 150, 205, 260, 350};

static int16_t band_x(const color_state_t *state, int band)
{
    return state->bands == RESISTOR_MAX_BANDS ? s_band_x5[band] : s_band_x4[band];
}

static void set_defaults(color_state_t *state, int bands)
{
    static const calc_color_t four[] = {COLOR_YELLOW, COLOR_VIOLET, COLOR_RED, COLOR_GOLD};
    static const calc_color_t five[] = {COLOR_BROWN, COLOR_BLACK, COLOR_BLACK, COLOR_RED, COLOR_BROWN};
    state->bands = bands;
    for (int i = 0; i < bands; ++i)
        state->color[i] = bands == RESISTOR_MAX_BANDS ? five[i] : four[i];
    state->selected = 0;
}

static void refresh(color_state_t *state)
{
    double ohms = 0.0, tolerance = 0.0;
    if (calc_resistor(state->bands, state->color, &ohms, &tolerance)) {
        char number[UI_LINE_MAX];
        if (ohms == 0.0) {
            ui_lines_printf(&state->view, COLOR_LINE_VALUE, "0 Ω");
        } else {
            const int unit = calc_best_unit(CALC_QTY_RESISTANCE, ohms);
            (void)calc_format(calc_convert(CALC_QTY_RESISTANCE, ohms, calc_default_unit(CALC_QTY_RESISTANCE), unit),
                              number, sizeof(number));
            ui_lines_printf(&state->view, COLOR_LINE_VALUE, "%s %s", number,
                            calc_unit(CALC_QTY_RESISTANCE, unit)->symbol);
        }
        (void)calc_format(tolerance, number, sizeof(number));
        ui_lines_printf(&state->view, COLOR_LINE_TOLERANCE, "±%s %%", number);
    } else {
        ui_lines_printf(&state->view, COLOR_LINE_VALUE, "-");
        ui_lines_printf(&state->view, COLOR_LINE_TOLERANCE, "-");
    }
    ui_lines_printf(&state->view, COLOR_LINE_BAND, "BANDA %d: %s", state->selected + 1,
                    s_kind_name[calc_band_kind(state->bands, state->selected)]);
}

void color_init(color_state_t *state)
{
    ui_lines_init(&state->view, COLOR_LINE_COUNT);
    set_defaults(state, RESISTOR_MIN_BANDS);
    refresh(state);
}

static ui_rect_t toggle_rect(int index)
{
    return (ui_rect_t){(int16_t)(index * TOGGLE_WIDTH), TOGGLE_TOP, TOGGLE_WIDTH, TOGGLE_HEIGHT};
}

static void palette_center(int color, int *cx, int *cy)
{
    *cx = PALETTE_FIRST_X + (color % PALETTE_COLUMNS) * PALETTE_PITCH_X;
    *cy = PALETTE_FIRST_Y + (color / PALETTE_COLUMNS) * PALETTE_PITCH_Y;
}

void color_tap(color_state_t *state, int32_t x, int32_t y)
{
    for (int i = 0; i < 2; ++i) {
        if (ui_rect_contains(toggle_rect(i), x, y)) {
            const int bands = i == 0 ? RESISTOR_MIN_BANDS : RESISTOR_MAX_BANDS;
            if (bands != state->bands) set_defaults(state, bands);
            refresh(state);
            return;
        }
    }
    for (int band = 0; band < state->bands; ++band) {
        const int left = band_x(state, band) - BAND_HIT_MARGIN;
        if (x >= left && x < left + BAND_WIDTH + 2 * BAND_HIT_MARGIN && y >= BAND_TOP - 6 &&
            y < BAND_TOP + BAND_HEIGHT + 6) {
            state->selected = band;
            refresh(state);
            return;
        }
    }
    for (int color = 0; color < COLOR_COUNT; ++color) {
        int cx, cy;
        palette_center(color, &cx, &cy);
        if (abs((int)x - cx) > PALETTE_CELL_HALF_W || abs((int)y - cy) > PALETTE_CELL_HALF_H) continue;
        const calc_band_kind_t kind = calc_band_kind(state->bands, state->selected);
        if (!calc_band_allows(kind, (calc_color_t)color)) return;
        state->color[state->selected] = (calc_color_t)color;
        if (state->selected < state->bands - 1) ++state->selected;
        refresh(state);
        return;
    }
}

static void draw_resistor(const color_state_t *state)
{
    const Color body = {205, 170, 120, 255};
    const Color lead = {160, 160, 170, 255};
    /* Leads first: the rounded body covers their inner ends. */
    DrawRectangle(BODY_X - LEAD_WIDTH, LEAD_Y, LEAD_WIDTH, LEAD_THICKNESS, lead);
    DrawRectangle(BODY_X + BODY_WIDTH, LEAD_Y, LEAD_WIDTH, LEAD_THICKNESS, lead);
    DrawRectangle(BODY_X, BODY_Y, BODY_WIDTH, BODY_HEIGHT, body);
    DrawCircle(BODY_X, BODY_Y + BODY_HEIGHT / 2, END_RADIUS, body);
    DrawCircle(BODY_X + BODY_WIDTH, BODY_Y + BODY_HEIGHT / 2, END_RADIUS, body);
    for (int band = 0; band < state->bands; ++band) {
        const ui_rect_t rect = {band_x(state, band), BAND_TOP, BAND_WIDTH, BAND_HEIGHT};
        ui_fill(rect, s_color[state->color[band]]);
        if (state->selected == band) ui_outline(rect, 3, UI_COLOR_ACCENT);
    }
}

static void draw_palette(const color_state_t *state)
{
    const calc_band_kind_t kind = calc_band_kind(state->bands, state->selected);
    for (int color = 0; color < COLOR_COUNT; ++color) {
        int cx, cy;
        palette_center(color, &cx, &cy);
        const bool allowed = calc_band_allows(kind, (calc_color_t)color);
        const bool chosen = state->color[state->selected] == (calc_color_t)color;
        DrawCircle(cx, cy, PALETTE_RADIUS + 3, chosen ? UI_COLOR_ACCENT : UI_COLOR_LINE);
        DrawCircle(cx, cy, PALETTE_RADIUS, allowed ? s_color[color] : UI_COLOR_PANEL);
        if (!allowed) DrawCircle(cx, cy, 6, UI_COLOR_LINE);
        ui_text_draw_center(s_color_name[color], cx, cy + LABEL_OFFSET, 2,
                            allowed ? UI_COLOR_MUTED : UI_COLOR_LINE);
    }
}

void color_draw(const color_state_t *state, const ui_pointer_t *pointer)
{
    ui_button(toggle_rect(0), "4 BANDE", 2,
              state->bands == RESISTOR_MIN_BANDS ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(toggle_rect(1), "5 BANDE", 2,
              state->bands == RESISTOR_MAX_BANDS ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    draw_resistor(state);
    ui_text_draw_center(ui_lines_get(&state->view, COLOR_LINE_VALUE), 240, VALUE_Y, VALUE_SCALE,
                        UI_COLOR_TEXT);
    char info[UI_LINE_MAX * 2];
    (void)snprintf(info, sizeof(info), "%s   %s", ui_lines_get(&state->view, COLOR_LINE_TOLERANCE),
                   ui_lines_get(&state->view, COLOR_LINE_BAND));
    ui_text_draw_center(info, 240, INFO_Y, INFO_SCALE, UI_COLOR_MUTED);
    draw_palette(state);
    ui_text_draw_center("TOCCA UNA BANDA PER SCEGLIERLA", 240, FOOTER_Y, 2, UI_COLOR_LINE);
}
