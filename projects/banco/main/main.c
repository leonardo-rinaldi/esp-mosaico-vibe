// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include <math.h>
#include "bundle_gsp.h"
#include "bsp/battery.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game.h"
#include "game_config.h"
#include "mosaico_game_app.h"
#include "mosaico_game_save.h"

/* Owned by app_main for the application lifetime. Engine callbacks execute on
 * that same task; touch samples reach it through the engine event queue. */
static game_handle_t s_game;

/* Remembered values live in NVS. Written from the engine's loop task only (on_start,
 * on_update), delayed by the debounce so typing does not wear the flash. */
static mosaico_save_t s_save;
static const char *const SAVE_NAMESPACE = "banco";
static const char *const SAVE_KEY = "settings";
enum { SAVE_DEBOUNCE_MS = 1500 };

/* Battery: a small task owns the fuel gauge and publishes one sample every few seconds,
 * so the I2C reads never delay a frame. on_update picks the sample up. The gauge is
 * attached here, not at startup, so a slow or missing gauge cannot hold back the first
 * frame; a failed attach is retried. */
enum {
    BATTERY_START_DELAY_MS = 3000,      /* Let the first frame and the health check go first. */
    BATTERY_POLL_MS = 2000,
    BATTERY_RETRY_MS = 10000,
    BATTERY_TASK_STACK = 4096,
    BATTERY_TASK_PRIORITY = 3,
    DECI_PER_UNIT = 10,
};
static battery_sample_t s_battery;      /* Written by battery_task, read by on_update, under the lock. */
static bool s_battery_fresh;
static portMUX_TYPE s_battery_lock = portMUX_INITIALIZER_UNLOCKED;

static int16_t to_deci(float value)
{
    const float scaled = value * DECI_PER_UNIT;
    if (scaled > INT16_MAX) return INT16_MAX;
    if (scaled < INT16_MIN) return INT16_MIN;
    return (int16_t)lroundf(scaled);
}

static void battery_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(BATTERY_START_DELAY_MS));
    bool online = false;
    while (true) {
        if (!online) {
            online = bsp_battery_init() == ESP_OK;
            if (!online) {
                ESP_LOGW(GAME_NAME, "battery gauge not available; retrying");
                vTaskDelay(pdMS_TO_TICKS(BATTERY_RETRY_MS));
                continue;
            }
        }
        bsp_battery_status_t status;
        if (bsp_battery_read(&status) == ESP_OK) {
            const battery_sample_t sample = {
                .valid = true,
                .percent = status.state_of_charge,
                .voltage_mv = status.voltage_mv,
                .current_ma = status.current_ma,
                .temperature_dc = to_deci(status.temperature_c),
                .minutes_to_empty = status.time_to_empty_min,
                .minutes_to_full = status.time_to_full_min,
            };
            portENTER_CRITICAL(&s_battery_lock);
            s_battery = sample;
            s_battery_fresh = true;
            portEXIT_CRITICAL(&s_battery_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(BATTERY_POLL_MS));
    }
}

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

/* Runs after the engine started NVS and before the first frame. A missing or
 * damaged record just leaves the defaults: the app must always start. */
static esp_err_t on_start(void)
{
    if (xTaskCreate(battery_task, "battery", BATTERY_TASK_STACK, NULL, BATTERY_TASK_PRIORITY, NULL) != pdPASS)
        ESP_LOGW(GAME_NAME, "battery task not started");
    const mosaico_save_config_t config = {
        .nvs_namespace = SAVE_NAMESPACE, .key = SAVE_KEY, .version = SETTINGS_VERSION,
        .payload_size = sizeof(settings_t), .debounce_ms = SAVE_DEBOUNCE_MS,
    };
    if (mosaico_save_init(&s_save, &config) != ESP_OK) return ESP_OK;
    settings_t settings, defaults;
    game_settings_get(s_game, &defaults);
    if (mosaico_save_load(&s_save, &settings, &defaults) == ESP_OK) game_settings_apply(s_game, &settings);
    return ESP_OK;
}

static void on_event(const mosaico_device_event_t *event)
{
    if (event && event->type == MOSAICO_DEVICE_EVENT_POINTER)
        game_set_pointer(s_game, event->x, event->y, event->pressed);
}

static void on_update(void)
{
    game_update(s_game);
    battery_sample_t battery;
    bool fresh;
    portENTER_CRITICAL(&s_battery_lock);
    battery = s_battery;
    fresh = s_battery_fresh;
    s_battery_fresh = false;
    portEXIT_CRITICAL(&s_battery_lock);
    if (fresh) game_set_battery(s_game, &battery);
    settings_t settings;
    if (game_settings_poll(s_game, &settings)) (void)mosaico_save_request(&s_save, &settings, now_ms());
    (void)mosaico_save_flush(&s_save, now_ms(), false);
}

static void on_render(void)
{
    (void)game_render(s_game);
}

static const mosaico_game_app_config_t s_config = {
    .tag = GAME_NAME,
    .window_title = GAME_NAME,
    .canvas_bind = GSP_GAME_BIND_GAME_CANVAS,
    .touch_points = 1,
    .enable_imu = true,         /* Without this the engine never starts the accelerometer. */
    .imu_sample_ms = 40,        /* 25 readings a second are plenty for the level. */
    .target_fps = GAME_TICK_HZ,
    .gsp_bundle = gsp_bundle_config,
    .on_start = on_start,
    .on_event = on_event,
    .on_update = on_update,
    .on_render = on_render,
};

void app_main(void)
{
    const game_config_t config = {GAME_WIDTH, GAME_HEIGHT};
    ESP_ERROR_CHECK(game_create(&config, &s_game) ? ESP_OK : ESP_ERR_NO_MEM);
    /* The engine starts iris_ota_support_start() and marks the image healthy
     * after presenting its first frame; the OTA writer remains in Recovery. */
    esp_err_t result = mosaico_game_app_run(&s_config);
    game_delete(s_game);
    s_game = NULL;
    ESP_ERROR_CHECK(result);
}
