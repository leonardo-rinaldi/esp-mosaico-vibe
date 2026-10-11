// SPDX-License-Identifier: Apache-2.0
/* Display text in Italian and English. The current language is a process-wide
 * setting owned by the single game loop (Host and device each run one game), so
 * screens can look their text up while drawing. The built-in font has no
 * apostrophe or accents on letters, so every string uses plain A-Z. */
#pragma once

typedef enum { LANG_IT, LANG_EN, LANG_COUNT } lang_t;

typedef enum {
    TXT_MENU_SUBTITLE,
    TXT_TILE_CONV_NAME, TXT_TILE_CONV_NOTE,
    TXT_TILE_OHM_NAME, TXT_TILE_OHM_NOTE,
    TXT_TILE_COLOR_NAME, TXT_TILE_COLOR_NOTE,
    TXT_TILE_CABLE_NAME, TXT_TILE_CABLE_NOTE,
    TXT_TILE_LEVEL_NAME, TXT_TILE_LEVEL_NOTE,
    TXT_TILE_LENS_NAME, TXT_TILE_LENS_NOTE,
    TXT_TITLE_CONV, TXT_TITLE_OHM, TXT_TITLE_COLOR, TXT_TITLE_CABLE, TXT_TITLE_LEVEL, TXT_TITLE_LENS,
    TXT_CONV_VALUE,
    TXT_OHM_ENTER_TWO, TXT_OHM_TOO_MANY, TXT_OHM_IMPOSSIBLE,
    TXT_COLOR_4_BANDS, TXT_COLOR_5_BANDS, TXT_COLOR_BAND_WORD, TXT_COLOR_FOOTER,
    TXT_KIND_DIGIT, TXT_KIND_MULTIPLIER, TXT_KIND_TOLERANCE,
    TXT_CABLE_CURRENT, TXT_CABLE_LENGTH, TXT_CABLE_MAX_DROP,
    TXT_CABLE_ENTER_VALUES, TXT_CABLE_OVER, TXT_CABLE_NO_SECTION,
    TXT_CABLE_COPPER, TXT_CABLE_ALUMINIUM, TXT_CABLE_SINGLE_PHASE, TXT_CABLE_THREE_PHASE,
    TXT_CABLE_MIN_SECTION,
    TXT_LEVEL_FLAT, TXT_LEVEL_EDGE, TXT_LEVEL_ZERO, TXT_LEVEL_RESET,
    TXT_LEVEL_NO_DATA, TXT_LEVEL_IS_LEVEL, TXT_LEVEL_IS_PLUMB,
    TXT_TITLE_BATTERY,
    TXT_BATT_NO_DATA, TXT_BATT_ABSENT, TXT_BATT_CHARGING, TXT_BATT_DISCHARGING, TXT_BATT_IDLE,
    TXT_BATT_FULL, TXT_BATT_VOLTAGE, TXT_BATT_CURRENT, TXT_BATT_TEMPERATURE, TXT_BATT_TIME_LEFT,
    TXT_BATT_TIME_TO_FULL, TXT_BATT_LOW, TXT_BATT_CRITICAL, TXT_BATT_HOURS, TXT_BATT_MINUTES,
    TXT_LENS_FREEZE, TXT_LENS_RESUME, TXT_LENS_FROZEN, TXT_LENS_STARTING,
    TXT_LENS_NO_CAMERA, TXT_LENS_NO_CAMERA_HINT, TXT_LENS_ERROR,
    TXT_COUNT
} text_id_t;

enum { I18N_COLOR_COUNT = 12 };     /* Black .. silver, in calc_color_t order. */

void i18n_set(lang_t lang);         /* Out-of-range values are ignored. */
lang_t i18n_get(void);
const char *i18n_text(text_id_t id);
/* Short colour label for the resistor palette; "" for an out-of-range colour. */
const char *i18n_color_name(int color);
