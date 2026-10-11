// SPDX-License-Identifier: Apache-2.0
/* Battery readings as the bench assistant uses them, and how they are judged.
 * Pure functions: the device task fills a battery_sample_t from the fuel gauge,
 * the Host tests fill it by hand. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;                 /* A reading has been received. */
    uint8_t percent;            /* State of charge from the gauge, 0..100 (clamped). */
    int16_t voltage_mv;
    int16_t current_ma;         /* Positive while charging, negative while the battery supplies. */
    int16_t temperature_dc;     /* Tenths of a degree Celsius. */
    uint16_t minutes_to_empty;  /* 0xFFFF when the gauge cannot tell. */
    uint16_t minutes_to_full;   /* 0xFFFF when the gauge cannot tell. */
} battery_sample_t;

typedef enum {
    BATTERY_UNKNOWN,            /* No reading yet. */
    BATTERY_ABSENT,             /* The gauge answers but no cell is connected. */
    BATTERY_CHARGING,
    BATTERY_DISCHARGING,
    BATTERY_IDLE,               /* Connected and neither charging nor supplying. */
} battery_state_t;

typedef enum { BATTERY_LEVEL_OK, BATTERY_LEVEL_LOW, BATTERY_LEVEL_CRITICAL } battery_level_t;

enum {
    BATTERY_PRESENT_MIN_MV = 2500,      /* Below this there is no cell on the gauge pins. */
    BATTERY_CURRENT_DEADBAND_MA = 5,    /* Smaller currents count as idle. */
    BATTERY_LOW_PERCENT = 20,
    BATTERY_CRITICAL_PERCENT = 10,
    BATTERY_FULL_PERCENT = 95,
    BATTERY_TIME_UNKNOWN = 0xFFFF,
};

battery_state_t battery_state(const battery_sample_t *sample);
/* Level from the percentage; BATTERY_LEVEL_OK when the state has no battery to judge. */
battery_level_t battery_level(const battery_sample_t *sample);
/* True when a time to empty or full is worth showing for the current state. */
bool battery_minutes_remaining(const battery_sample_t *sample, uint16_t *minutes);
