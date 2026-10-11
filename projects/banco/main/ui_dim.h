// SPDX-License-Identifier: Apache-2.0
/* Software dimming. The panel's own brightness register cannot be changed while the
 * display driver is sending frames (the call hangs the screen), so the interface dims
 * itself: every colour it draws is scaled by the current brightness. On an OLED panel a
 * darker picture is also a lower power draw, and black pixels cost almost nothing.
 * Included by the headers that expose drawing, so no drawing call can skip it. */
#pragma once
#include "mosaico_raylib_fast.h"

/* Brightness in percent, 0..100 (0 draws everything black). */
void ui_dim_set(int percent);
Color ui_dim_color(Color color);

#undef ClearBackground
#undef DrawRectangle
#undef DrawCircle
#define ClearBackground(color) MosaicoFastClearBackground(ui_dim_color(color))
#define DrawRectangle(x, y, width, height, color) MosaicoFastDrawRectangle(x, y, width, height, ui_dim_color(color))
#define DrawCircle(x, y, radius, color) MosaicoFastDrawCircle(x, y, radius, ui_dim_color(color))
