// SPDX-License-Identifier: Apache-2.0
/* Lens: a live camera picture with digital zoom and a freeze button, from the camera
 * module in the left slot. This file is the portable part (state, touches, buttons,
 * texts). The picture itself is drawn by lens_present(), which has one version for
 * the Host simulator (a test pattern) and one for the board (the camera frame). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ui_widgets.h"

typedef enum {
    LENS_STARTING,          /* Looking for the camera. */
    LENS_LIVE,
    LENS_NO_CAMERA,         /* Nothing found in the left slot. */
    LENS_ERROR,             /* Found, but it stopped delivering pictures. */
} lens_status_t;

enum { LENS_ZOOM_MIN = 1, LENS_ZOOM_MAX = 4 };

/* What the camera side has to do, polled by the board loop. */
typedef struct {
    bool active;            /* The lens screen is on; otherwise the camera sleeps. */
    uint8_t zoom;           /* 1..4 */
    bool frozen;
} lens_request_t;

/* View lines: 0 the zoom ("X2"), 1 the status text (empty while live and not frozen),
 * 2 the hint under it (empty unless there is no camera), 3 the freeze button label. */
enum { LENS_LINE_ZOOM, LENS_LINE_STATUS, LENS_LINE_HINT, LENS_LINE_FREEZE, LENS_LINE_COUNT };

typedef struct {
    lens_status_t status;
    uint8_t zoom;
    bool frozen;
    ui_lines_t view;
} lens_state_t;

void lens_init(lens_state_t *state);
/* Called when the screen opens: unfrozen, looking for the camera. Keeps the zoom. */
void lens_enter(lens_state_t *state);
void lens_relocalize(lens_state_t *state);
void lens_set_status(lens_state_t *state, lens_status_t status);
void lens_tap(lens_state_t *state, int32_t x, int32_t y);
/* Buttons and texts drawn over the picture (the back header is drawn by the caller). */
void lens_draw_overlay(const lens_state_t *state, const ui_pointer_t *pointer);

/* Draw one whole frame: the picture, then `overlay(context)` on top. Returns false
 * when no frame could be drawn. */
bool lens_present(const lens_state_t *state, void (*overlay)(void *context), void *context);
