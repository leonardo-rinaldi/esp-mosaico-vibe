// SPDX-License-Identifier: Apache-2.0
#include "power_status.h"

battery_state_t battery_state(const battery_sample_t *sample)
{
    if (!sample || !sample->valid) return BATTERY_UNKNOWN;
    if (sample->voltage_mv < BATTERY_PRESENT_MIN_MV) return BATTERY_ABSENT;
    if (sample->current_ma > BATTERY_CURRENT_DEADBAND_MA) return BATTERY_CHARGING;
    if (sample->current_ma < -BATTERY_CURRENT_DEADBAND_MA) return BATTERY_DISCHARGING;
    return BATTERY_IDLE;
}

battery_level_t battery_level(const battery_sample_t *sample)
{
    const battery_state_t state = battery_state(sample);
    if (state == BATTERY_UNKNOWN || state == BATTERY_ABSENT) return BATTERY_LEVEL_OK;
    if (sample->percent < BATTERY_CRITICAL_PERCENT) return BATTERY_LEVEL_CRITICAL;
    if (sample->percent < BATTERY_LOW_PERCENT) return BATTERY_LEVEL_LOW;
    return BATTERY_LEVEL_OK;
}

bool battery_minutes_remaining(const battery_sample_t *sample, uint16_t *minutes)
{
    if (!minutes) return false;
    const battery_state_t state = battery_state(sample);
    uint16_t value = BATTERY_TIME_UNKNOWN;
    if (state == BATTERY_DISCHARGING) value = sample->minutes_to_empty;
    else if (state == BATTERY_CHARGING) value = sample->minutes_to_full;
    if (value == BATTERY_TIME_UNKNOWN) return false;
    *minutes = value;
    return true;
}
