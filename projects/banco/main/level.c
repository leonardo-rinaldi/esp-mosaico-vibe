// SPDX-License-Identifier: Apache-2.0
#include "level.h"
#include <math.h>

enum {
    LEVEL_SIGN_X = 1,       /* Flip to -1 if a tilt shows the wrong way on the device. */
    LEVEL_SIGN_Y = 1,
};
#define RAD_TO_DEG (180.0 / 3.14159265358979323846)
#define SLOPE_MAX_PERCENT 9999.0
#define SLOPE_LIMIT_DEG 89.0

static bool has_gravity(double ax, double ay, double az)
{
    return sqrt(ax * ax + ay * ay + az * az) >= LEVEL_MIN_GRAVITY_G;
}

bool level_flat_angles(double ax, double ay, double az, double *roll_deg, double *pitch_deg)
{
    if (!roll_deg || !pitch_deg || !has_gravity(ax, ay, az)) return false;
    /* The sign of az tells face up from face down; folding keeps the angle small
     * for a board lying upside down too. */
    const double up = az < 0.0 ? -az : az;
    *roll_deg = atan2(LEVEL_SIGN_X * ax, up) * RAD_TO_DEG;
    *pitch_deg = atan2(LEVEL_SIGN_Y * ay, up) * RAD_TO_DEG;
    return true;
}

bool level_edge_angle(double ax, double ay, double az, double *angle_deg)
{
    if (!angle_deg || !has_gravity(ax, ay, az)) return false;
    double angle = atan2(LEVEL_SIGN_X * ax, LEVEL_SIGN_Y * ay) * RAD_TO_DEG;
    /* A line has no direction: fold the half turn so up and down read the same. */
    if (angle > 90.0) angle -= 180.0;
    if (angle < -90.0) angle += 180.0;
    *angle_deg = angle;
    return true;
}

double level_slope_percent(double angle_deg)
{
    const double magnitude = fabs(angle_deg);
    if (magnitude >= SLOPE_LIMIT_DEG) return angle_deg < 0.0 ? -SLOPE_MAX_PERCENT : SLOPE_MAX_PERCENT;
    return tan(angle_deg / RAD_TO_DEG) * 100.0;
}
