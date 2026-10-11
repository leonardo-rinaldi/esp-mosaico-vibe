// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "esp_check.h"
#include "esp_timer.h"
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

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

/* Runs after the engine started NVS and before the first frame. A missing or
 * damaged record just leaves the defaults: the app must always start. */
static esp_err_t on_start(void)
{
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
    .imu_sample_ms = 20,
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
