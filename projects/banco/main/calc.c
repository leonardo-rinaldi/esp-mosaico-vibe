// SPDX-License-Identifier: Apache-2.0
#include "calc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Units ------------------------------------------------------------ */

static const calc_unit_t s_units[CALC_QTY_COUNT][CALC_UNITS_PER_QTY] = {
    [CALC_QTY_RESISTANCE] = {{"mΩ", 1e-3}, {"Ω", 1.0}, {"kΩ", 1e3}, {"MΩ", 1e6}},
    [CALC_QTY_VOLTAGE] = {{"µV", 1e-6}, {"mV", 1e-3}, {"V", 1.0}, {"kV", 1e3}},
    [CALC_QTY_CURRENT] = {{"µA", 1e-6}, {"mA", 1e-3}, {"A", 1.0}, {"kA", 1e3}},
    [CALC_QTY_CAPACITANCE] = {{"pF", 1e-12}, {"nF", 1e-9}, {"µF", 1e-6}, {"mF", 1e-3}},
    [CALC_QTY_POWER] = {{"mW", 1e-3}, {"W", 1.0}, {"kW", 1e3}, {"MW", 1e6}},
};

static const int s_default_unit[CALC_QTY_COUNT] = {1, 2, 2, 2, 1};

const calc_unit_t *calc_unit(calc_qty_t qty, int index)
{
    if ((unsigned)qty >= CALC_QTY_COUNT || (unsigned)index >= CALC_UNITS_PER_QTY) return NULL;
    return &s_units[qty][index];
}

int calc_default_unit(calc_qty_t qty)
{
    return (unsigned)qty < CALC_QTY_COUNT ? s_default_unit[qty] : 0;
}

double calc_convert(calc_qty_t qty, double value, int from, int to)
{
    const calc_unit_t *a = calc_unit(qty, from);
    const calc_unit_t *b = calc_unit(qty, to);
    if (!a || !b) return NAN;
    /* Divide by the larger factor where possible: 4700 / 1e3 is exact enough,
     * 4700 * 1e-3 is not always. */
    return a->factor >= b->factor ? value * (a->factor / b->factor)
                                  : value / (b->factor / a->factor);
}

int calc_best_unit(calc_qty_t qty, double base_value)
{
    const double magnitude = fabs(base_value);
    if (!(magnitude > 0.0) || !isfinite(magnitude)) return 0;
    for (int i = CALC_UNITS_PER_QTY - 1; i >= 0; --i) {
        const double shown = magnitude / s_units[qty][i].factor;
        if (shown >= 1.0 - 1e-9) return shown < 1000.0 - 1e-9 ? i : (i + 1 < CALC_UNITS_PER_QTY ? i + 1 : i);
    }
    return 0;
}

/* ---- Number entry ----------------------------------------------------- */

void calc_entry_clear(calc_entry_t *entry)
{
    entry->text[0] = '\0';
}

bool calc_entry_is_empty(const calc_entry_t *entry)
{
    return entry->text[0] == '\0';
}

static size_t entry_length(const calc_entry_t *entry)
{
    return strlen(entry->text);
}

static bool entry_has_point(const calc_entry_t *entry)
{
    return strchr(entry->text, ',') != NULL;
}

static char *entry_digits_start(calc_entry_t *entry)
{
    return entry->text[0] == '-' ? entry->text + 1 : entry->text;
}

bool calc_entry_digit(calc_entry_t *entry, char digit)
{
    if (digit < '0' || digit > '9') return false;
    size_t length = entry_length(entry);
    char *digits = entry_digits_start(entry);
    if (strcmp(digits, "0") == 0) { /* "0" then a digit replaces the zero: no "007". */
        digits[0] = digit;
        return true;
    }
    if (length >= CALC_ENTRY_MAX) return false;
    entry->text[length] = digit;
    entry->text[length + 1] = '\0';
    return true;
}

bool calc_entry_double_zero(calc_entry_t *entry)
{
    char *digits = entry_digits_start(entry);
    if (digits[0] == '\0') return calc_entry_digit(entry, '0');
    if (strcmp(digits, "0") == 0) return false;
    if (entry_length(entry) + 2 > CALC_ENTRY_MAX) return false;
    return calc_entry_digit(entry, '0') && calc_entry_digit(entry, '0');
}

bool calc_entry_point(calc_entry_t *entry)
{
    if (entry_has_point(entry)) return false;
    if (*entry_digits_start(entry) == '\0' && !calc_entry_digit(entry, '0')) return false;
    size_t length = entry_length(entry);
    if (length >= CALC_ENTRY_MAX) return false;
    entry->text[length] = ',';
    entry->text[length + 1] = '\0';
    return true;
}

bool calc_entry_backspace(calc_entry_t *entry)
{
    size_t length = entry_length(entry);
    if (!length) return false;
    entry->text[length - 1] = '\0';
    if (strcmp(entry->text, "-") == 0) entry->text[0] = '\0';
    return true;
}

bool calc_entry_toggle_sign(calc_entry_t *entry)
{
    size_t length = entry_length(entry);
    if (entry->text[0] == '-') {
        memmove(entry->text, entry->text + 1, length); /* Includes the terminator. */
        return true;
    }
    if (length >= CALC_ENTRY_MAX) return false;
    memmove(entry->text + 1, entry->text, length + 1);
    entry->text[0] = '-';
    return true;
}

bool calc_entry_value(const calc_entry_t *entry, double *value)
{
    char text[CALC_ENTRY_MAX + 1];
    bool has_digit = false;
    for (size_t i = 0; entry->text[i]; ++i) {
        text[i] = entry->text[i] == ',' ? '.' : entry->text[i];
        text[i + 1] = '\0';
        has_digit |= entry->text[i] >= '0' && entry->text[i] <= '9';
    }
    if (!has_digit) return false;
    if (value) *value = strtod(text, NULL);
    return true;
}

/* ---- Number display --------------------------------------------------- */

static size_t put(char *out, size_t size, size_t at, char ch)
{
    if (at + 1 < size) out[at] = ch;
    return at + 1;
}

size_t calc_format(double value, char *out, size_t size)
{
    if (!out || !size) return 0;
    out[0] = '\0';
    if (isnan(value)) { snprintf(out, size, "-"); return strlen(out); }
    if (isinf(value)) { snprintf(out, size, value < 0 ? "-inf" : "inf"); return strlen(out); }
    if (value == 0.0) { snprintf(out, size, "0"); return 1; }

    char raw[40];
    snprintf(raw, sizeof(raw), "%.7g", value);
    size_t at = 0;
    const char *p = raw;
    if (*p == '-') { at = put(out, size, at, '-'); ++p; }
    const char *exp_at = strchr(p, 'e');
    size_t mantissa_length = exp_at ? (size_t)(exp_at - p) : strlen(p);
    const char *point = memchr(p, '.', mantissa_length);
    size_t int_length = point ? (size_t)(point - p) : mantissa_length;

    for (size_t i = 0; i < int_length; ++i) {
        if (int_length >= 5 && i > 0 && (int_length - i) % 3 == 0) at = put(out, size, at, ' ');
        at = put(out, size, at, p[i]);
    }
    if (point) {
        at = put(out, size, at, ',');
        for (const char *q = point + 1; q < p + mantissa_length; ++q) at = put(out, size, at, *q);
    }
    if (exp_at) {
        at = put(out, size, at, 'e');
        const char *q = exp_at + 1;
        if (*q == '-') { at = put(out, size, at, '-'); }
        if (*q == '-' || *q == '+') ++q;
        while (*q == '0' && q[1]) ++q;
        for (; *q; ++q) at = put(out, size, at, *q);
    }
    out[at < size ? at : size - 1] = '\0';
    return strlen(out);
}

size_t calc_entry_display(const calc_entry_t *entry, char *out, size_t size)
{
    if (!out || !size) return 0;
    out[0] = '\0';
    if (!entry) return 0;
    size_t at = 0;
    const char *p = entry->text;
    if (*p == '-') { at = put(out, size, at, '-'); ++p; }
    size_t int_length = 0;
    while (p[int_length] >= '0' && p[int_length] <= '9') ++int_length;
    for (size_t i = 0; i < int_length; ++i) {
        if (int_length >= 5 && i > 0 && (int_length - i) % 3 == 0) at = put(out, size, at, ' ');
        at = put(out, size, at, p[i]);
    }
    for (const char *q = p + int_length; *q; ++q) at = put(out, size, at, *q);
    out[at < size ? at : size - 1] = '\0';
    return strlen(out);
}

/* ---- Ohm's law -------------------------------------------------------- */

static double checked_div(double a, double b)
{
    return b == 0.0 ? NAN : a / b;
}

static double checked_sqrt(double a)
{
    return a < 0.0 ? NAN : sqrt(a);
}

ohm_status_t calc_ohm(const bool known[OHM_COUNT], const double in[OHM_COUNT],
                      double out[OHM_COUNT])
{
    int count = 0;
    for (int i = 0; i < OHM_COUNT; ++i) {
        out[i] = known[i] ? in[i] : NAN;
        count += known[i];
    }
    if (count != 2) return OHM_NEED_TWO;

    const bool v = known[OHM_V], i = known[OHM_I], r = known[OHM_R], p = known[OHM_P];
    const double V = in[OHM_V], I = in[OHM_I], R = in[OHM_R], P = in[OHM_P];
    if (v && i) {
        out[OHM_R] = checked_div(V, I);
        out[OHM_P] = V * I;
    } else if (v && r) {
        out[OHM_I] = checked_div(V, R);
        out[OHM_P] = checked_div(V * V, R);
    } else if (v && p) {
        out[OHM_I] = checked_div(P, V);
        out[OHM_R] = checked_div(V * V, P);
    } else if (i && r) {
        out[OHM_V] = I * R;
        out[OHM_P] = I * I * R;
    } else if (i && p) {
        out[OHM_V] = checked_div(P, I);
        out[OHM_R] = checked_div(P, I * I);
    } else if (r && p) {
        out[OHM_V] = checked_sqrt(P * R);
        out[OHM_I] = checked_sqrt(checked_div(P, R));
    }
    for (int k = 0; k < OHM_COUNT; ++k)
        if (isnan(out[k])) return OHM_UNDEFINED;
    return OHM_OK;
}

/* ---- Resistor colour code -------------------------------------------- */

calc_band_kind_t calc_band_kind(int bands, int index)
{
    const int digits = bands - 2;       /* 2 digit bands on 4 bands, 3 on 5. */
    if (index < digits) return BAND_DIGIT;
    return index == digits ? BAND_MULTIPLIER : BAND_TOLERANCE;
}

static double tolerance_of(calc_color_t color)
{
    switch (color) {
    case COLOR_BROWN: return 1.0;
    case COLOR_RED: return 2.0;
    case COLOR_GREEN: return 0.5;
    case COLOR_BLUE: return 0.25;
    case COLOR_VIOLET: return 0.1;
    case COLOR_GREY: return 0.05;
    case COLOR_GOLD: return 5.0;
    case COLOR_SILVER: return 10.0;
    default: return NAN;
    }
}

bool calc_band_allows(calc_band_kind_t kind, calc_color_t color)
{
    switch (kind) {
    case BAND_DIGIT: return color >= COLOR_BLACK && color <= COLOR_WHITE;
    case BAND_MULTIPLIER: return color >= COLOR_BLACK && color <= COLOR_SILVER;
    case BAND_TOLERANCE: return !isnan(tolerance_of(color));
    }
    return false;
}

bool calc_resistor(int bands, const calc_color_t colors[RESISTOR_MAX_BANDS],
                   double *ohms, double *tolerance_percent)
{
    if (bands < RESISTOR_MIN_BANDS || bands > RESISTOR_MAX_BANDS) return false;
    int mantissa = 0;
    double multiplier = 1.0;
    for (int i = 0; i < bands; ++i) {
        const calc_band_kind_t kind = calc_band_kind(bands, i);
        if (!calc_band_allows(kind, colors[i])) return false;
        if (kind == BAND_DIGIT) {
            mantissa = mantissa * 10 + (int)colors[i];
        } else if (kind == BAND_MULTIPLIER) {
            multiplier = colors[i] == COLOR_GOLD ? 0.1
                       : colors[i] == COLOR_SILVER ? 0.01 : pow(10.0, (int)colors[i]);
        }
    }
    if (ohms) *ohms = mantissa * multiplier;
    if (tolerance_percent) *tolerance_percent = tolerance_of(colors[bands - 1]);
    return true;
}

/* ---- Cable voltage drop ---------------------------------------------- */

/* Standard conductor sections in mm². */
static const double s_sections[] = {
    1.5, 2.5, 4, 6, 10, 16, 25, 35, 50, 70, 95, 120, 150, 185, 240,
};

/* Resistivity in ohm·mm²/m at a 70 °C conductor temperature (conservative). */
static const double s_resistivity[] = {[CABLE_COPPER] = 0.0225, [CABLE_ALUMINIUM] = 0.036};

int calc_cable_section_count(void)
{
    return (int)(sizeof(s_sections) / sizeof(s_sections[0]));
}

double calc_cable_section(int index)
{
    return (unsigned)index < (unsigned)calc_cable_section_count() ? s_sections[index] : NAN;
}

double calc_cable_nominal_voltage(cable_system_t system)
{
    return system == CABLE_THREE_PHASE ? 400.0 : 230.0;
}

double calc_cable_drop_v(const cable_input_t *input, double section_mm2)
{
    if (!input || !(section_mm2 > 0.0)) return NAN;
    /* Resistive line drop, cos φ = 1: 2 conductors single-phase, √3 three-phase. */
    const double factor = input->system == CABLE_THREE_PHASE ? sqrt(3.0) : 2.0;
    return factor * s_resistivity[input->material] * input->length_m * input->current_a
           / section_mm2;
}

int calc_cable_min_section(const cable_input_t *input, double *drop_v, double *drop_percent)
{
    if (!input) return -1;
    const double nominal = calc_cable_nominal_voltage(input->system);
    for (int i = 0; i < calc_cable_section_count(); ++i) {
        const double drop = calc_cable_drop_v(input, s_sections[i]);
        const double percent = drop / nominal * 100.0;
        if (percent <= input->limit_percent) {
            if (drop_v) *drop_v = drop;
            if (drop_percent) *drop_percent = percent;
            return i;
        }
    }
    return -1;
}
