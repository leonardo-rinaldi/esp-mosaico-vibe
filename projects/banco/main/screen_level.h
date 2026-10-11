// SPDX-License-Identifier: Apache-2.0
/* Spirit level and inclinometer from the board's accelerometer. Two modes:
 * FLAT (board lying on a surface, bubble vial with X/Y angles) and EDGE (board
 * held against a pipe or wall, one angle from vertical). A zero button stores
 * the current tilt as the reference for the mode. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "level.h"
#include "ui_widgets.h"

typedef enum { LEVEL_MODE_FLAT, LEVEL_MODE_EDGE, LEVEL_MODE_COUNT } level_mode_t;

/* View lines: 0 and 1 the angles ("+1,2°"; the second is empty in EDGE mode),
 * 2 and 3 their slopes in percent, 4 the status or error text. All empty while
 * there is no sensor data. */
enum {
    LEVEL_LINE_ANGLE_A, LEVEL_LINE_ANGLE_B, LEVEL_LINE_SLOPE_A, LEVEL_LINE_SLOPE_B,
    LEVEL_LINE_HINT, LEVEL_LINE_COUNT,
};

enum { LEVEL_AXES = 2, LEVEL_ZERO_LIMIT = 9000 };    /* Zero offsets are within +-90 degrees. */

typedef struct {
    level_mode_t mode;
    bool valid;                         /* A gravity sample has been received. */
    double smooth[3];                   /* Low-pass filtered acceleration, g. */
    double raw[LEVEL_AXES];             /* Angles before the zero offset. */
    double shown[LEVEL_AXES];           /* Angles after the zero offset. */
    int16_t zero[LEVEL_MODE_COUNT][LEVEL_AXES];  /* Zero offsets in 0.01 degree. */
    ui_lines_t view;
} level_state_t;

void level_init(level_state_t *state);
void level_relocalize(level_state_t *state);
/* Feed one acceleration sample (g); called once per tick. */
void level_update(level_state_t *state, double ax, double ay, double az);
void level_tap(level_state_t *state, int32_t x, int32_t y);
void level_draw(const level_state_t *state, const ui_pointer_t *pointer);
