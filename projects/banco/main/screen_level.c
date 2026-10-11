// SPDX-License-Identifier: Apache-2.0
#include "screen_level.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "i18n.h"
#include "ui_font.h"

enum {
    MODE_TOP = 50, MODE_HEIGHT = 40, MODE_WIDTH = 240,
    BUTTON_TOP = 414, BUTTON_HEIGHT = 48, BUTTON_WIDTH = 240,
    VIAL_X = 240, VIAL_Y = 206, VIAL_RADIUS = 100, VIAL_RING = 4,
    BUBBLE_RADIUS = 13, TARGET_RADIUS = 18, CROSS_THICKNESS = 2,
    READOUT_Y = 318, READOUT_SCALE = 3, SLOPE_Y = 350, HINT_Y = 380,
    COLUMN_A_X = 150, COLUMN_B_X = 370, AXIS_LABEL_OFFSET = 88,
    EDGE_ANGLE_Y = 130, EDGE_ANGLE_SCALE = 7, EDGE_SLOPE_Y = 300,
    GAUGE_Y = 224, GAUGE_LEFT = 24, GAUGE_WIDTH = 432, GAUGE_HEIGHT = 18, MARKER_WIDTH = 10,
    MARKER_OVERSHOOT = 8,
};

#define SMOOTHING 0.25              /* Weight of each new sample. */
#define LEVEL_TOLERANCE_DEG 0.5     /* Within this the board counts as level. */
#define BUBBLE_FULL_SCALE_DEG 10.0  /* Tilt that pushes the bubble to the vial edge. */
#define GAUGE_FULL_SCALE_DEG 45.0
#define CENTIDEG 100.0
#define ZERO_LIMIT_CENTIDEG 9000.0
#define SHOWN_ZERO_THRESHOLD 0.05   /* Below this the angle prints as 0,0 without a sign. */

static const Color COLOR_VIAL_FLUID = {40, 70, 60, 255};

static double clamp(double value, double low, double high)
{
    return value < low ? low : (value > high ? high : value);
}

static int16_t to_centideg(double degrees)
{
    return (int16_t)lround(clamp(degrees * CENTIDEG, -ZERO_LIMIT_CENTIDEG, ZERO_LIMIT_CENTIDEG));
}

/* "+1,2°" with a decimal comma; tiny values print as "0,0°". */
static void format_angle(double degrees, char *out, size_t size)
{
    const double magnitude = fabs(degrees);
    const char *sign = magnitude < SHOWN_ZERO_THRESHOLD ? "" : (degrees < 0.0 ? "-" : "+");
    char number[16];
    (void)snprintf(number, sizeof(number), "%.1f", magnitude);
    for (char *p = number; *p; ++p)
        if (*p == '.') *p = ',';
    (void)snprintf(out, size, "%s%s°", sign, number);
}

static void format_slope(double degrees, char *out, size_t size)
{
    const double percent = level_slope_percent(degrees);
    char number[16];
    (void)snprintf(number, sizeof(number), "%.1f", fabs(percent));
    for (char *p = number; *p; ++p)
        if (*p == '.') *p = ',';
    (void)snprintf(out, size, "%s%s %%", fabs(percent) < SHOWN_ZERO_THRESHOLD ? "" : (percent < 0.0 ? "-" : "+"),
                   number);
}

static int axis_count(const level_state_t *state)
{
    return state->mode == LEVEL_MODE_FLAT ? 2 : 1;
}

static bool is_level(const level_state_t *state)
{
    for (int i = 0; i < axis_count(state); ++i)
        if (fabs(state->shown[i]) > LEVEL_TOLERANCE_DEG) return false;
    return true;
}

static void refresh_text(level_state_t *state)
{
    for (int line = 0; line < LEVEL_LINE_COUNT; ++line) ui_lines_printf(&state->view, line, "%s", "");
    if (!state->valid) {
        ui_lines_printf(&state->view, LEVEL_LINE_HINT, "%s", i18n_text(TXT_LEVEL_NO_DATA));
        return;
    }
    char text[UI_LINE_MAX];
    for (int i = 0; i < axis_count(state); ++i) {
        format_angle(state->shown[i], text, sizeof(text));
        ui_lines_printf(&state->view, LEVEL_LINE_ANGLE_A + i, "%s", text);
        format_slope(state->shown[i], text, sizeof(text));
        ui_lines_printf(&state->view, LEVEL_LINE_SLOPE_A + i, "%s", text);
    }
    if (is_level(state))
        ui_lines_printf(&state->view, LEVEL_LINE_HINT, "%s",
                        i18n_text(state->mode == LEVEL_MODE_FLAT ? TXT_LEVEL_IS_LEVEL : TXT_LEVEL_IS_PLUMB));
}

void level_relocalize(level_state_t *state)
{
    refresh_text(state);
}

void level_init(level_state_t *state)
{
    memset(state, 0, sizeof(*state));
    ui_lines_init(&state->view, LEVEL_LINE_COUNT);
    state->mode = LEVEL_MODE_FLAT;
    refresh_text(state);
}

void level_update(level_state_t *state, double ax, double ay, double az)
{
    double angle[LEVEL_AXES] = {0.0, 0.0};
    const bool ok = state->mode == LEVEL_MODE_FLAT
                        ? level_flat_angles(ax, ay, az, &angle[0], &angle[1])
                        : level_edge_angle(ax, ay, az, &angle[0]);
    if (!ok) {
        if (state->valid) {
            state->valid = false;
            refresh_text(state);
        }
        return;
    }
    const double sample[3] = {ax, ay, az};
    for (int i = 0; i < 3; ++i)
        state->smooth[i] = state->valid ? state->smooth[i] + SMOOTHING * (sample[i] - state->smooth[i])
                                        : sample[i];
    state->valid = true;
    if (state->mode == LEVEL_MODE_FLAT)
        (void)level_flat_angles(state->smooth[0], state->smooth[1], state->smooth[2], &state->raw[0],
                                &state->raw[1]);
    else
        (void)level_edge_angle(state->smooth[0], state->smooth[1], state->smooth[2], &state->raw[0]);
    for (int i = 0; i < LEVEL_AXES; ++i)
        state->shown[i] = i < axis_count(state) ? state->raw[i] - state->zero[state->mode][i] / CENTIDEG : 0.0;
    refresh_text(state);
}

static ui_rect_t mode_rect(int index)
{
    return (ui_rect_t){(int16_t)(index * MODE_WIDTH), MODE_TOP, MODE_WIDTH, MODE_HEIGHT};
}

static ui_rect_t zero_rect(int index)
{
    return (ui_rect_t){(int16_t)(index * BUTTON_WIDTH), BUTTON_TOP, BUTTON_WIDTH, BUTTON_HEIGHT};
}

void level_tap(level_state_t *state, int32_t x, int32_t y)
{
    for (int mode = 0; mode < LEVEL_MODE_COUNT; ++mode) {
        if (!ui_rect_contains(mode_rect(mode), x, y)) continue;
        if ((level_mode_t)mode != state->mode) {
            state->mode = (level_mode_t)mode;
            state->valid = false;           /* Angles are recomputed on the next sample. */
            refresh_text(state);
        }
        return;
    }
    if (ui_rect_contains(zero_rect(0), x, y)) {
        if (!state->valid) return;
        for (int i = 0; i < axis_count(state); ++i) {
            state->zero[state->mode][i] = to_centideg(state->raw[i]);
            state->shown[i] = state->raw[i] - state->zero[state->mode][i] / CENTIDEG;
        }
        refresh_text(state);
    } else if (ui_rect_contains(zero_rect(1), x, y)) {
        memset(state->zero[state->mode], 0, sizeof(state->zero[state->mode]));
        for (int i = 0; i < axis_count(state); ++i) state->shown[i] = state->raw[i];
        refresh_text(state);
    }
}

static void draw_vial(const level_state_t *state)
{
    const bool level = state->valid && is_level(state);
    DrawCircle(VIAL_X, VIAL_Y, VIAL_RADIUS + VIAL_RING, level ? UI_COLOR_OK : UI_COLOR_LINE);
    DrawCircle(VIAL_X, VIAL_Y, VIAL_RADIUS, level ? UI_COLOR_OK_BG : COLOR_VIAL_FLUID);
    DrawRectangle(VIAL_X - VIAL_RADIUS, VIAL_Y - CROSS_THICKNESS / 2, 2 * VIAL_RADIUS, CROSS_THICKNESS,
                  UI_COLOR_LINE);
    DrawRectangle(VIAL_X - CROSS_THICKNESS / 2, VIAL_Y - VIAL_RADIUS, CROSS_THICKNESS, 2 * VIAL_RADIUS,
                  UI_COLOR_LINE);
    DrawCircle(VIAL_X, VIAL_Y, TARGET_RADIUS + 2, UI_COLOR_LINE);
    DrawCircle(VIAL_X, VIAL_Y, TARGET_RADIUS, level ? UI_COLOR_OK_BG : COLOR_VIAL_FLUID);
    if (!state->valid) return;
    const double reach = VIAL_RADIUS - BUBBLE_RADIUS - 2;
    double dx = state->shown[0] / BUBBLE_FULL_SCALE_DEG * reach;
    double dy = state->shown[1] / BUBBLE_FULL_SCALE_DEG * reach;
    const double length = sqrt(dx * dx + dy * dy);
    if (length > reach) {                   /* Keep the bubble inside the vial. */
        dx *= reach / length;
        dy *= reach / length;
    }
    DrawCircle(VIAL_X + (int)lround(dx), VIAL_Y + (int)lround(dy), BUBBLE_RADIUS,
               level ? UI_COLOR_OK : UI_COLOR_ACCENT);
}

static void draw_flat(const level_state_t *state)
{
    draw_vial(state);
    if (!state->valid) return;
    const Color color = is_level(state) ? UI_COLOR_OK : UI_COLOR_TEXT;
    ui_text_draw("X", COLUMN_A_X - AXIS_LABEL_OFFSET, READOUT_Y + 4, 2, UI_COLOR_MUTED);
    ui_text_draw("Y", COLUMN_B_X - AXIS_LABEL_OFFSET, READOUT_Y + 4, 2, UI_COLOR_MUTED);
    ui_text_draw_center(ui_lines_get(&state->view, LEVEL_LINE_ANGLE_A), COLUMN_A_X, READOUT_Y,
                        READOUT_SCALE, color);
    ui_text_draw_center(ui_lines_get(&state->view, LEVEL_LINE_ANGLE_B), COLUMN_B_X, READOUT_Y,
                        READOUT_SCALE, color);
    ui_text_draw_center(ui_lines_get(&state->view, LEVEL_LINE_SLOPE_A), COLUMN_A_X, SLOPE_Y, 2,
                        UI_COLOR_MUTED);
    ui_text_draw_center(ui_lines_get(&state->view, LEVEL_LINE_SLOPE_B), COLUMN_B_X, SLOPE_Y, 2,
                        UI_COLOR_MUTED);
}

static void draw_edge(const level_state_t *state)
{
    const ui_rect_t track = {GAUGE_LEFT, GAUGE_Y, GAUGE_WIDTH, GAUGE_HEIGHT};
    ui_fill(track, UI_COLOR_PANEL);
    ui_outline(track, 2, UI_COLOR_LINE);
    const int center = GAUGE_LEFT + GAUGE_WIDTH / 2;
    DrawRectangle(center - 1, GAUGE_Y - MARKER_OVERSHOOT, 3, GAUGE_HEIGHT + 2 * MARKER_OVERSHOOT, UI_COLOR_LINE);
    if (!state->valid) return;
    const bool level = is_level(state);
    const Color color = level ? UI_COLOR_OK : UI_COLOR_TEXT;
    const double fraction = clamp(state->shown[0] / GAUGE_FULL_SCALE_DEG, -1.0, 1.0);
    const int x = center + (int)lround(fraction * (GAUGE_WIDTH / 2 - MARKER_WIDTH));
    DrawRectangle(x - MARKER_WIDTH / 2, GAUGE_Y - MARKER_OVERSHOOT, MARKER_WIDTH,
                  GAUGE_HEIGHT + 2 * MARKER_OVERSHOOT, level ? UI_COLOR_OK : UI_COLOR_ACCENT);
    ui_text_draw_center(ui_lines_get(&state->view, LEVEL_LINE_ANGLE_A), 240, EDGE_ANGLE_Y,
                        EDGE_ANGLE_SCALE, color);
    ui_text_draw_center(ui_lines_get(&state->view, LEVEL_LINE_SLOPE_A), 240, EDGE_SLOPE_Y, 3,
                        UI_COLOR_MUTED);
}

void level_draw(const level_state_t *state, const ui_pointer_t *pointer)
{
    ui_button(mode_rect(0), i18n_text(TXT_LEVEL_FLAT), 2,
              state->mode == LEVEL_MODE_FLAT ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    ui_button(mode_rect(1), i18n_text(TXT_LEVEL_EDGE), 2,
              state->mode == LEVEL_MODE_EDGE ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
    if (state->mode == LEVEL_MODE_FLAT) draw_flat(state);
    else draw_edge(state);
    const char *hint = ui_lines_get(&state->view, LEVEL_LINE_HINT);
    if (hint && *hint)
        ui_text_draw_center(hint, 240, HINT_Y, 2, state->valid ? UI_COLOR_OK : UI_COLOR_BAD);
    ui_button(zero_rect(0), i18n_text(TXT_LEVEL_ZERO), 2, UI_BUTTON_FUNCTION, pointer);
    ui_button(zero_rect(1), i18n_text(TXT_LEVEL_RESET), 2, UI_BUTTON_FUNCTION, pointer);
}
