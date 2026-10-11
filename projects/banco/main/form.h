// SPDX-License-Identifier: Apache-2.0
/* Shared pieces of the bench tool screens (LED resistor, divider, SMD codes, AWG):
 * labelled number fields and number formatting with a unit prefix. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

enum { FORM_FIELD_PITCH = 44, FORM_FIELD_HEIGHT = 40, FORM_VALUE_RIGHT = 418, FORM_UNIT_X = 428 };

ui_rect_t form_field_rect(int top, int index);
/* Index of the field under (x, y) among `count` fields starting at `top`, or -1. */
int form_field_hit(int top, int count, int32_t x, int32_t y);
void form_draw_field(ui_rect_t rect, const char *label, const char *value, const char *unit, bool selected);
/* Rounds to `digits` significant digits. */
double form_round_significant(double value, int digits);
/* "4,7 kΩ": a base-unit value in the unit that reads best; "-" for NaN or infinity. */
void form_format_quantity(calc_qty_t qty, double base_value, char *out, size_t size);
/* Button row of `count` equal buttons across the screen at `top`. */
ui_rect_t form_button_rect(int top, int height, int count, int index);
