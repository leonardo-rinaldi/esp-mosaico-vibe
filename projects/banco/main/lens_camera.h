// SPDX-License-Identifier: Apache-2.0
/* Board only: the camera module task. It owns the camera and keeps the latest picture
 * (480 x 480, RGB565, already zoomed) in a buffer the display loop can copy from. The
 * camera only runs while the lens screen is on. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "screen_lens.h"

/* Start the task; the camera itself stays off until a request says active. */
bool lens_camera_start(void);
/* Tell the task what the screen wants; cheap, call it every tick. */
void lens_camera_request(const lens_request_t *request);
lens_status_t lens_camera_status(void);
/* The newest picture, or NULL when there is none. Pair with lens_camera_release();
 * copy and release quickly, the camera task waits for it. */
const uint16_t *lens_camera_acquire(void);
void lens_camera_release(void);
