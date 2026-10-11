// SPDX-License-Identifier: Apache-2.0
/* Bench helpers: AWG wire sizes, LED series resistor, voltage divider and SMD part
 * codes. Pure functions, no drawing and no engine, so they build and test on a host
 * compiler. Values are in base units (Ω, V, A, W) unless the name says otherwise. */
#pragma once
#include <stdbool.h>

/* ---- AWG --------------------------------------------------------------- */

enum { AWG_MIN = -3, AWG_MAX = 40 };    /* -1, -2, -3 are 00, 000, 0000. */

double awg_diameter_mm(int awg);
double awg_area_mm2(int awg);
/* The AWG whose area is closest (in ratio) to `mm2`; AWG_MAX or AWG_MIN beyond the table. */
int awg_nearest(double mm2);
/* Smallest metric standard section (mm²) that is at least `mm2`, or 0 when it is larger
 * than the biggest one in the list. */
double metric_section_at_least(double mm2);

/* ---- Standard values and ratings --------------------------------------- */

/* Smallest E24 value that is at least `value` (any decade); 0 for a value <= 0. */
double e24_at_least(double value);
/* Nearest E24 value (ratio-wise); used to suggest a part for a computed result. */
double e24_nearest(double value);
/* Label of the smallest common power rating that is at least twice `watts` ("1/4 W"),
 * or ">5 W". */
const char *power_rating_label(double watts);

/* ---- LED series resistor ------------------------------------------------ */

typedef struct {
    double supply_v, led_v, current_a;
} led_input_t;

typedef struct {
    double ideal_ohm;       /* Exactly the wanted current. */
    double std_ohm;         /* Next E24 value up, so the current never exceeds the wanted one. */
    double actual_a;        /* Current with the standard value. */
    double power_w;         /* Dissipated in the resistor. */
} led_result_t;

/* False when the supply does not exceed the LED voltage or an input is not positive. */
bool calc_led_resistor(const led_input_t *input, led_result_t *out);

/* ---- Voltage divider ---------------------------------------------------- */

/* vin * r2 / (r1 + r2); NaN when r1 + r2 is not positive. */
double divider_vout(double vin, double r1, double r2);
/* r2 that gives `vout` from `vin` with `r1`; false unless 0 < vout < vin and r1 > 0. */
bool divider_r2(double vin, double vout, double r1, double *r2);

/* ---- SMD codes ---------------------------------------------------------- */

typedef enum { SMD_OK, SMD_INCOMPLETE, SMD_INVALID } smd_status_t;

enum { EIA96_COUNT = 96 };
double eia96_base(int code);                /* 01..96 -> 100..976, 0 when out of range. */
double eia96_multiplier(char letter);       /* Z Y R X S A B H C D E F; 0 for another letter. */

/* Typed text: digits and at most one ',' standing for the "R" of codes such as 4R7.
 * Resistors: 3 digits (two digits and a power of ten), 4 digits (three digits and a power of
 * ten) or R notation. */
smd_status_t smd_resistor(const char *text, double *ohms);
/* EIA-96: two digits (01..96) and a multiplier letter. */
smd_status_t smd_eia96(const char *digits, char letter, double *ohms);
/* Capacitors: 3 digits (two digits and a power of ten, in pF) or R notation (pF). */
smd_status_t smd_capacitor(const char *text, double *picofarads);
