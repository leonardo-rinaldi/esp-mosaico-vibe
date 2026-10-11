// SPDX-License-Identifier: Apache-2.0
/* Board version of lens_present(): the camera frame is copied straight into the
 * display frame, then the buttons are drawn over it with the same widgets. */
#include <string.h>
#include "lens_camera.h"
#include "mosaico_raylib_port.h"
#include "screen_lens.h"

bool lens_present(const lens_state_t *state, void (*overlay)(void *context), void *context)
{
    (void)state;
    uint16_t *pixels = NULL;
    size_t stride = 0;
    if (mosaico_raylib_port_begin_frame(&pixels, &stride) != ESP_OK || !pixels) return false;
    ui_surface_set(pixels, stride);
    const uint16_t *picture = lens_camera_acquire();
    if (picture) {
        for (int row = 0; row < UI_SURFACE_SIZE; ++row)
            memcpy(pixels + (size_t)row * stride, picture + (size_t)row * UI_SURFACE_SIZE,
                   UI_SURFACE_SIZE * sizeof(uint16_t));
        lens_camera_release();
    } else {
        ui_fill((ui_rect_t){0, 0, UI_SURFACE_SIZE, UI_SURFACE_SIZE}, UI_COLOR_BG);
    }
    overlay(context);
    ui_surface_set(NULL, 0);
    return mosaico_raylib_port_present_frame() == ESP_OK;
}
