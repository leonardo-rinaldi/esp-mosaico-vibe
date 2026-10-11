// SPDX-License-Identifier: Apache-2.0
/* Pure calculation helpers for the bench assistant: no drawing, no globals, no
 * dependency on the engine, so they build and test on a plain host compiler.
 * Strings are UTF-8 (units such as "kΩ" and "µF"). */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* ---- Units ------------------------------------------------------------ */

typedef enum {
    CALC_QTY_RESISTANCE,
    CALC_QTY_VOLTAGE,
    CALC_QTY_CURRENT,
    CALC_QTY_CAPACITANCE,
    CALC_QTY_POWER,
    CALC_QTY_COUNT,
} calc_qty_t;

typedef struct {
    const char *symbol;
    double factor;          /* Value of one unit in the base unit (Ω, V, A, F, W). */
} calc_unit_t;

enum { CALC_UNITS_PER_QTY = 4 };

const calc_unit_t *calc_unit(calc_qty_t qty, int index);
int calc_default_unit(calc_qty_t qty);                  /* Ω, V, A, µF, W */
double calc_convert(calc_qty_t qty, double value, int from, int to);
/* Index of the unit that shows |value_in_base| as 1..999.9 (smallest prefix when
 * the value is zero or outside every range). */
int calc_best_unit(calc_qty_t qty, double base_value);

/* ---- Number entry and display ---------------------------------------- */

enum { CALC_ENTRY_MAX = 10 };       /* Characters, excluding the terminator. */

typedef struct {
    char text[CALC_ENTRY_MAX + 1];  /* "-12,5": ',' is the decimal mark. */
} calc_entry_t;

void calc_entry_clear(calc_entry_t *entry);
bool calc_entry_is_empty(const calc_entry_t *entry);
bool calc_entry_digit(calc_entry_t *entry, char digit);      /* '0'..'9' */
bool calc_entry_double_zero(calc_entry_t *entry);
bool calc_entry_point(calc_entry_t *entry);
bool calc_entry_backspace(calc_entry_t *entry);
bool calc_entry_toggle_sign(calc_entry_t *entry);
/* False while the text holds no digit (empty, "-" or ","). */
bool calc_entry_value(const calc_entry_t *entry, double *value);

/* Up to 7 significant digits, ',' decimal mark, spaces between thousands for
 * integers of five or more digits, "e-5" style exponent for tiny or huge values. */
size_t calc_format(double value, char *out, size_t size);
/* The typed text with spaces between thousands for integers of five or more
 * digits ("12345,6" becomes "12 345,6"). */
size_t calc_entry_display(const calc_entry_t *entry, char *out, size_t size);

/* ---- Ohm's law and power (DC) ----------------------------------------- */

typedef enum { OHM_V, OHM_I, OHM_R, OHM_P, OHM_COUNT } ohm_var_t;

typedef enum {
    OHM_OK,
    OHM_NEED_TWO,       /* Exactly two known values are required. */
    OHM_UNDEFINED,      /* Division by zero or a square root of a negative number. */
} ohm_status_t;

/* Fills out[] for all four variables (known ones are copied). Entries that
 * cannot be computed are NaN. */
ohm_status_t calc_ohm(const bool known[OHM_COUNT], const double in[OHM_COUNT],
                      double out[OHM_COUNT]);

/* ---- Resistor colour code -------------------------------------------- */

typedef enum {
    COLOR_BLACK, COLOR_BROWN, COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN,
    COLOR_BLUE, COLOR_VIOLET, COLOR_GREY, COLOR_WHITE, COLOR_GOLD, COLOR_SILVER,
    COLOR_COUNT,
} calc_color_t;

typedef enum { BAND_DIGIT, BAND_MULTIPLIER, BAND_TOLERANCE } calc_band_kind_t;

enum { RESISTOR_MIN_BANDS = 4, RESISTOR_MAX_BANDS = 5 };

calc_band_kind_t calc_band_kind(int bands, int index);
bool calc_band_allows(calc_band_kind_t kind, calc_color_t color);
/* Value in ohms and tolerance in percent; false if a colour is not valid for its band. */
bool calc_resistor(int bands, const calc_color_t colors[RESISTOR_MAX_BANDS],
                   double *ohms, double *tolerance_percent);

/* ---- Cable voltage drop ---------------------------------------------- */

typedef enum { CABLE_COPPER, CABLE_ALUMINIUM } cable_material_t;
typedef enum { CABLE_SINGLE_PHASE, CABLE_THREE_PHASE } cable_system_t;

typedef struct {
    double current_a;
    double length_m;                /* One-way length of the line. */
    double limit_percent;
    cable_material_t material;
    cable_system_t system;
} cable_input_t;

int calc_cable_section_count(void);
double calc_cable_section(int index);                    /* mm² */
double calc_cable_nominal_voltage(cable_system_t system); /* 230 V or 400 V */
/* Resistive drop in volts at the given conductor section. */
double calc_cable_drop_v(const cable_input_t *input, double section_mm2);
/* Smallest standard section whose drop stays within the limit, or -1 if none
 * does. Reports that section's drop in volts and percent when found. */
int calc_cable_min_section(const cable_input_t *input, double *drop_v, double *drop_percent);
