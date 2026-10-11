// SPDX-License-Identifier: Apache-2.0
/* Host simulator version of lens_present(): there is no camera, so the picture is a
 * test pattern whose squares grow with the zoom. */
#include "screen_lens.h"

enum { CELL = 30, PICTURE_TOP = 44, PICTURE_BOTTOM = 424 };

bool lens_present(const lens_state_t *state, void (*overlay)(void *context), void *context)
{
    BeginDrawing();
    if (!MosaicoFastFrameAvailable()) {
        EndDrawing();
        return false;
    }
    ClearBackground(UI_COLOR_BG);
    const int cell = CELL * state->zoom;
    const Color dark = {40, 60, 90, 255};
    const Color light = {90, 130, 170, 255};
    for (int row = 0; PICTURE_TOP + row * cell < PICTURE_BOTTOM; ++row) {
        for (int col = 0; col * cell < UI_SURFACE_SIZE; ++col) {
            const int top = PICTURE_TOP + row * cell;
            const int height = top + cell > PICTURE_BOTTOM ? PICTURE_BOTTOM - top : cell;
            DrawRectangle(col * cell, top, cell, height, (row + col) % 2 ? dark : light);
        }
    }
    overlay(context);
    EndDrawing();
    return true;
}
