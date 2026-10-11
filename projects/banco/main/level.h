// SPDX-License-Identifier: Apache-2.0
/* Tilt maths for the spirit level: pure functions, no hardware access.
 * Acceleration is in g as the IMU reports it. Which way each board axis points
 * is fixed by the LEVEL_SIGN_* constants in level.c. */
#pragma once
#include <stdbool.h>

/* Below this magnitude (in g) the sample is not gravity: the sensor sent nothing. */
#define LEVEL_MIN_GRAVITY_G 0.5

/* Board lying on its back: tilt about the board's two axes, in degrees.
 * Returns false when the sample holds no usable gravity. */
bool level_flat_angles(double ax, double ay, double az, double *roll_deg, double *pitch_deg);

/* Board standing on an edge against a pipe or wall, screen facing the user:
 * deviation of the screen's vertical edge from true vertical, -90..90 degrees. */
bool level_edge_angle(double ax, double ay, double az, double *angle_deg);

/* Slope in percent for an angle in degrees (rise over run); clamped for angles
 * close to 90 degrees. */
double level_slope_percent(double angle_deg);
