// SPDX-License-Identifier: Apache-2.0
#include "screen_lens.h"
#include "i18n.h"
#include "ui_font.h"

enum {
    BAR_TOP = 424, BAR_HEIGHT = 56,
    MINUS_X = 0, MINUS_W = 100, LABEL_X = 100, LABEL_W = 100, PLUS_X = 200, PLUS_W = 100,
    FREEZE_X = 300, FREEZE_W = 180,
    ZOOM_SCALE = 4, BUTTON_SCALE = 3, STATUS_SCALE = 2, HINT_SCALE = 2,
    STATUS_Y = 200, HINT_Y = 232, STRIP_PAD = 6,
};

static const Color COLOR_STRIP = {0, 0, 0, 255};

void lens_relocalize(lens_state_t *state)
{
    ui_lines_printf(&state->view, LENS_LINE_ZOOM, "X%d", state->zoom);
    const char *status = "";
    const char *hint = "";
    switch (state->status) {
    case LENS_STARTING: status = i18n_text(TXT_LENS_STARTING); break;
    case LENS_NO_CAMERA:
        status = i18n_text(TXT_LENS_NO_CAMERA);
        hint = i18n_text(TXT_LENS_NO_CAMERA_HINT);
        break;
    case LENS_ERROR: status = i18n_text(TXT_LENS_ERROR); break;
    case LENS_LIVE: if (state->frozen) status = i18n_text(TXT_LENS_FROZEN); break;
    }
    ui_lines_printf(&state->view, LENS_LINE_STATUS, "%s", status);
    ui_lines_printf(&state->view, LENS_LINE_HINT, "%s", hint);
    ui_lines_printf(&state->view, LENS_LINE_FREEZE, "%s",
                    i18n_text(state->frozen ? TXT_LENS_RESUME : TXT_LENS_FREEZE));
}

void lens_init(lens_state_t *state)
{
    state->status = LENS_STARTING;
    state->zoom = LENS_ZOOM_MIN;
    state->frozen = false;
    ui_lines_init(&state->view, LENS_LINE_COUNT);
    lens_relocalize(state);
}

void lens_enter(lens_state_t *state)
{
    state->status = LENS_STARTING;
    state->frozen = false;
    lens_relocalize(state);
}

void lens_set_status(lens_state_t *state, lens_status_t status)
{
    if (status == state->status) return;
    state->status = status;
    lens_relocalize(state);
}

static ui_rect_t button_rect(int x, int w)
{
    return (ui_rect_t){(int16_t)x, BAR_TOP, (int16_t)w, BAR_HEIGHT};
}

void lens_tap(lens_state_t *state, int32_t x, int32_t y)
{
    if (y >= BAR_TOP) {
        if (ui_rect_contains(button_rect(MINUS_X, MINUS_W), x, y) && state->zoom > LENS_ZOOM_MIN) --state->zoom;
        else if (ui_rect_contains(button_rect(PLUS_X, PLUS_W), x, y) && state->zoom < LENS_ZOOM_MAX) ++state->zoom;
        else if (ui_rect_contains(button_rect(FREEZE_X, FREEZE_W), x, y)) state->frozen = !state->frozen;
    } else {
        state->frozen = !state->frozen;     /* Tapping the picture freezes it too. */
    }
    lens_relocalize(state);
}

/* A black strip behind centred text keeps it readable on any picture. */
static void draw_strip_text(const char *text, int y, int scale, Color color)
{
    if (!text[0]) return;
    const int width = ui_text_width(text, scale);
    const int height = ui_text_height(scale);
    ui_fill((ui_rect_t){(int16_t)(UI_SURFACE_SIZE / 2 - width / 2 - STRIP_PAD), (int16_t)(y - STRIP_PAD),
                        (int16_t)(width + 2 * STRIP_PAD), (int16_t)(height + 2 * STRIP_PAD)}, COLOR_STRIP);
    ui_text_draw_center(text, UI_SURFACE_SIZE / 2, y, scale, color);
}

void lens_draw_overlay(const lens_state_t *state, const ui_pointer_t *pointer)
{
    draw_strip_text(ui_lines_get(&state->view, LENS_LINE_STATUS), STATUS_Y, STATUS_SCALE,
                    state->status == LENS_ERROR ? UI_COLOR_BAD : UI_COLOR_ACCENT);
    draw_strip_text(ui_lines_get(&state->view, LENS_LINE_HINT), HINT_Y, HINT_SCALE, UI_COLOR_MUTED);

    ui_fill((ui_rect_t){0, BAR_TOP, UI_SURFACE_SIZE, BAR_HEIGHT}, UI_COLOR_PANEL);
    ui_button(button_rect(MINUS_X, MINUS_W), "-", ZOOM_SCALE,
              state->zoom > LENS_ZOOM_MIN ? UI_BUTTON_FUNCTION : UI_BUTTON_DISABLED, pointer);
    ui_button(button_rect(PLUS_X, PLUS_W), "+", ZOOM_SCALE,
              state->zoom < LENS_ZOOM_MAX ? UI_BUTTON_FUNCTION : UI_BUTTON_DISABLED, pointer);
    ui_text_draw_center(ui_lines_get(&state->view, LENS_LINE_ZOOM), LABEL_X + LABEL_W / 2,
                        BAR_TOP + (BAR_HEIGHT - ui_text_height(ZOOM_SCALE)) / 2, ZOOM_SCALE, UI_COLOR_ACCENT);
    ui_button(button_rect(FREEZE_X, FREEZE_W), ui_lines_get(&state->view, LENS_LINE_FREEZE), BUTTON_SCALE,
              state->frozen ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL, pointer);
}
