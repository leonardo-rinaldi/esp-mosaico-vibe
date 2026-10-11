// SPDX-License-Identifier: Apache-2.0
#include "calc_bench.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

/* ---- AWG --------------------------------------------------------------- */

double awg_diameter_mm(int awg)
{
    return 0.127 * pow(92.0, (36.0 - awg) / 39.0);
}

double awg_area_mm2(int awg)
{
    const double d = awg_diameter_mm(awg);
    return PI / 4.0 * d * d;
}

int awg_nearest(double mm2)
{
    int best = AWG_MAX;
    double best_ratio = INFINITY;
    if (!(mm2 > 0.0)) return best;
    for (int awg = AWG_MIN; awg <= AWG_MAX; ++awg) {
        const double ratio = fabs(log(awg_area_mm2(awg) / mm2));
        if (ratio < best_ratio) {
            best_ratio = ratio;
            best = awg;
        }
    }
    return best;
}

static const double s_metric[] = {0.05, 0.14, 0.25, 0.34, 0.5, 0.75, 1, 1.5, 2.5, 4, 6, 10, 16, 25, 35, 50,
                                  70, 95, 120, 150, 185, 240, 300};

double metric_section_at_least(double mm2)
{
    for (size_t i = 0; i < sizeof(s_metric) / sizeof(s_metric[0]); ++i)
        if (s_metric[i] >= mm2 * (1.0 - 1e-9)) return s_metric[i];
    return 0.0;
}

/* ---- Standard values and ratings --------------------------------------- */

static const double s_e24[] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
                               3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
enum { E24_COUNT = sizeof(s_e24) / sizeof(s_e24[0]) };

double e24_at_least(double value)
{
    if (!(value > 0.0)) return 0.0;
    const double decade = pow(10.0, floor(log10(value)));
    for (int i = 0; i < E24_COUNT; ++i)
        if (s_e24[i] * decade >= value * (1.0 - 1e-9)) return s_e24[i] * decade;
    return 10.0 * decade;
}

double e24_nearest(double value)
{
    if (!(value > 0.0)) return 0.0;
    const double decade = pow(10.0, floor(log10(value)));
    double best = 10.0 * decade, best_ratio = fabs(log(best / value));
    for (int i = 0; i < E24_COUNT; ++i) {
        const double ratio = fabs(log(s_e24[i] * decade / value));
        if (ratio < best_ratio) {
            best_ratio = ratio;
            best = s_e24[i] * decade;
        }
    }
    return best;
}

const char *power_rating_label(double watts)
{
    static const struct { double watts; const char *label; } rating[] = {
        {0.125, "1/8 W"}, {0.25, "1/4 W"}, {0.5, "1/2 W"}, {1.0, "1 W"}, {2.0, "2 W"}, {5.0, "5 W"},
    };
    for (size_t i = 0; i < sizeof(rating) / sizeof(rating[0]); ++i)
        if (rating[i].watts >= 2.0 * watts * (1.0 - 1e-9)) return rating[i].label;
    return ">5 W";
}

/* ---- LED series resistor ------------------------------------------------ */

bool calc_led_resistor(const led_input_t *input, led_result_t *out)
{
    if (!input || !out) return false;
    if (!(input->supply_v > 0.0 && input->led_v > 0.0 && input->current_a > 0.0)) return false;
    const double drop = input->supply_v - input->led_v;
    if (!(drop > 0.0)) return false;
    out->ideal_ohm = drop / input->current_a;
    out->std_ohm = e24_at_least(out->ideal_ohm);
    out->actual_a = drop / out->std_ohm;
    out->power_w = drop * out->actual_a;
    return true;
}

/* ---- Voltage divider ---------------------------------------------------- */

double divider_vout(double vin, double r1, double r2)
{
    const double total = r1 + r2;
    return total > 0.0 ? vin * r2 / total : NAN;
}

bool divider_r2(double vin, double vout, double r1, double *r2)
{
    if (!r2 || !(r1 > 0.0 && vout > 0.0 && vout < vin)) return false;
    *r2 = r1 * vout / (vin - vout);
    return true;
}

/* ---- SMD codes ---------------------------------------------------------- */

static const short s_e96[EIA96_COUNT] = {
    100, 102, 105, 107, 110, 113, 115, 118, 121, 124, 127, 130, 133, 137, 140, 143,
    147, 150, 154, 158, 162, 165, 169, 174, 178, 182, 187, 191, 196, 200, 205, 210,
    215, 221, 226, 232, 237, 243, 249, 255, 261, 267, 274, 280, 287, 294, 301, 309,
    316, 324, 332, 340, 348, 357, 365, 374, 383, 392, 402, 412, 422, 432, 442, 453,
    464, 475, 487, 499, 511, 523, 536, 549, 562, 576, 590, 604, 619, 634, 649, 665,
    681, 698, 715, 732, 750, 768, 787, 806, 825, 845, 866, 887, 909, 931, 953, 976,
};

double eia96_base(int code)
{
    return code >= 1 && code <= EIA96_COUNT ? s_e96[code - 1] : 0.0;
}

double eia96_multiplier(char letter)
{
    switch (letter) {
    case 'Z': return 0.001;
    case 'Y': case 'R': return 0.01;
    case 'X': case 'S': return 0.1;
    case 'A': return 1.0;
    case 'B': case 'H': return 10.0;
    case 'C': return 100.0;
    case 'D': return 1e3;
    case 'E': return 1e4;
    case 'F': return 1e5;
    default: return 0.0;
    }
}

/* Power-of-ten digit of a printed code; 8 and 9 mean 0.01 and 0.1. */
static double code_power(int digit)
{
    return digit == 8 ? 0.01 : (digit == 9 ? 0.1 : pow(10.0, digit));
}

typedef struct { bool point; int digits; } text_shape_t;

/* Checks the text holds only digits and at most one ','; reports what it holds. */
static bool scan_text(const char *text, text_shape_t *shape)
{
    memset(shape, 0, sizeof(*shape));
    for (const char *p = text; *p; ++p) {
        if (*p == ',') {
            if (shape->point) return false;
            shape->point = true;
        } else if (*p >= '0' && *p <= '9') {
            ++shape->digits;
        } else {
            return false;
        }
    }
    return true;
}

static double parse_r_notation(const char *text)
{
    char copy[16];
    size_t length = strlen(text);
    if (length >= sizeof(copy)) length = sizeof(copy) - 1;
    memcpy(copy, text, length);
    copy[length] = '\0';
    for (char *p = copy; *p; ++p)
        if (*p == ',') *p = '.';
    return strtod(copy, NULL);
}

static smd_status_t decode(const char *text, int max_digits, bool allow_four, double *value)
{
    text_shape_t shape;
    if (!text || !value || !scan_text(text, &shape)) return SMD_INVALID;
    if (shape.digits == 0) return SMD_INCOMPLETE;
    if (shape.point) {
        *value = parse_r_notation(text);
        return SMD_OK;
    }
    if (shape.digits > max_digits) return SMD_INVALID;
    if (shape.digits < 3) return SMD_INCOMPLETE;
    if (shape.digits == 4 && !allow_four) return SMD_INVALID;
    const int length = shape.digits;
    int mantissa = 0;
    for (int i = 0; i < length - 1; ++i) mantissa = mantissa * 10 + (text[i] - '0');
    *value = mantissa * code_power(text[length - 1] - '0');
    return SMD_OK;
}

smd_status_t smd_resistor(const char *text, double *ohms)
{
    return decode(text, 4, true, ohms);
}

smd_status_t smd_capacitor(const char *text, double *picofarads)
{
    return decode(text, 3, false, picofarads);
}

smd_status_t smd_eia96(const char *digits, char letter, double *ohms)
{
    if (!digits || !ohms) return SMD_INVALID;
    const size_t length = strlen(digits);
    for (size_t i = 0; i < length; ++i)
        if (digits[i] < '0' || digits[i] > '9') return SMD_INVALID;
    if (length < 2) return SMD_INCOMPLETE;
    if (length > 2) return SMD_INVALID;
    const double base = eia96_base((digits[0] - '0') * 10 + (digits[1] - '0'));
    const double multiplier = eia96_multiplier(letter);
    if (base == 0.0 || multiplier == 0.0) return SMD_INVALID;
    *ohms = base * multiplier;
    return SMD_OK;
}
