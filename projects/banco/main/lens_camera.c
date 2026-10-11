// SPDX-License-Identifier: Apache-2.0
/* Board only. One task owns the camera module in the left slot. While the lens screen
 * is open it opens the camera, converts each UYVY frame to a centred, zoomed, rotated
 * 480 x 480 RGB565 picture with the PPA, and publishes it through a three-buffer
 * hand-over: the display loop reads the newest finished buffer while the task fills
 * another one. When the screen closes the camera is released. A missing or unplugged
 * module is reported and retried, so the module can be plugged in at any time. */
#include "lens_camera.h"
#include <inttypes.h>
#include <stddef.h>
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"
#include "mosaico_module_camera.h"

enum {
    PICTURE_SIZE = UI_SURFACE_SIZE,
    BUFFER_COUNT = 3,
    BUFFER_ALIGNMENT = 128,
    TASK_STACK = 8192,
    TASK_PRIORITY = 4,
    IDLE_POLL_MS = 100,
    RETRY_MS = 1000,
    FAILURE_LIMIT = 3,
    NO_BUFFER = -1,
    FROZEN_DISCARD_FRAMES = 2,
};

static const char *const TAG = "lens";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static lens_request_t s_request = {.zoom = LENS_ZOOM_MIN};
static lens_status_t s_status = LENS_STARTING;
static uint16_t *s_buffer[BUFFER_COUNT];
static size_t s_buffer_size;
static int s_front = NO_BUFFER;         /* Newest finished picture. Under s_lock. */
static int s_reading = NO_BUFFER;       /* Held by the display loop. Under s_lock. */
static ppa_client_handle_t s_ppa;
static mosaico_camera_handle_t s_camera;

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

static uint32_t align_down_even(uint32_t value)
{
    return value & ~1U;
}

static void set_status(lens_status_t status)
{
    portENTER_CRITICAL(&s_lock);
    s_status = status;
    portEXIT_CRITICAL(&s_lock);
}

void lens_camera_request(const lens_request_t *request)
{
    if (!request) return;
    portENTER_CRITICAL(&s_lock);
    s_request = *request;
    portEXIT_CRITICAL(&s_lock);
}

lens_status_t lens_camera_status(void)
{
    portENTER_CRITICAL(&s_lock);
    const lens_status_t status = s_status;
    portEXIT_CRITICAL(&s_lock);
    return status;
}

const uint16_t *lens_camera_acquire(void)
{
    const uint16_t *picture = NULL;
    portENTER_CRITICAL(&s_lock);
    if (s_front != NO_BUFFER) {
        s_reading = s_front;
        picture = s_buffer[s_reading];
    }
    portEXIT_CRITICAL(&s_lock);
    return picture;
}

void lens_camera_release(void)
{
    portENTER_CRITICAL(&s_lock);
    s_reading = NO_BUFFER;
    portEXIT_CRITICAL(&s_lock);
}

static lens_request_t current_request(void)
{
    portENTER_CRITICAL(&s_lock);
    const lens_request_t request = s_request;
    portEXIT_CRITICAL(&s_lock);
    return request;
}

/* A buffer that is neither on show nor being read. */
static int pick_back_buffer(void)
{
    int pick = NO_BUFFER;
    portENTER_CRITICAL(&s_lock);
    for (int i = 0; i < BUFFER_COUNT && pick == NO_BUFFER; ++i)
        if (i != s_front && i != s_reading) pick = i;
    portEXIT_CRITICAL(&s_lock);
    return pick;
}

static void publish(int index)
{
    portENTER_CRITICAL(&s_lock);
    s_front = index;
    s_status = LENS_LIVE;
    portEXIT_CRITICAL(&s_lock);
}

static void forget_picture(void)
{
    portENTER_CRITICAL(&s_lock);
    s_front = NO_BUFFER;
    portEXIT_CRITICAL(&s_lock);
}

static esp_err_t init_resources(void)
{
    s_buffer_size = align_up((size_t)PICTURE_SIZE * PICTURE_SIZE * sizeof(uint16_t), BUFFER_ALIGNMENT);
    const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        s_buffer[i] = heap_caps_aligned_calloc(BUFFER_ALIGNMENT, 1, s_buffer_size, caps);
        if (!s_buffer[i]) return ESP_ERR_NO_MEM;
    }
    const ppa_client_config_t config = {.oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1};
    return ppa_register_client(&config, &s_ppa);
}

/* Centre crop of 1/zoom of the frame's short side, rotated to the display's
 * orientation and scaled up to the full picture. */
static esp_err_t convert(const mosaico_camera_frame_t *frame, uint16_t *out, uint8_t zoom)
{
    if (!frame || !frame->data || frame->pixel_format != V4L2_PIX_FMT_UYVY) return ESP_ERR_NOT_SUPPORTED;
    const uint32_t shortest = frame->width < frame->height ? frame->width : frame->height;
    const uint32_t crop = align_down_even(shortest / (zoom ? zoom : 1));
    if (crop == 0) return ESP_ERR_INVALID_SIZE;
    const uint32_t bytes_per_line = frame->bytes_per_line ? frame->bytes_per_line : frame->width * 2U;
    if (bytes_per_line % 2U) return ESP_ERR_INVALID_SIZE;

    esp_err_t err = esp_cache_msync((void *)frame->data, frame->size,
                                    ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    if (err != ESP_OK) return err;
    err = esp_cache_msync(out, s_buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (err != ESP_OK) return err;

    const ppa_srm_oper_config_t operation = {
        .in = {
            .buffer = frame->data,
            .pic_w = bytes_per_line / 2U,
            .pic_h = frame->height,
            .block_w = crop,
            .block_h = crop,
            .block_offset_x = align_down_even((frame->width - crop) / 2U),
            .block_offset_y = align_down_even((frame->height - crop) / 2U),
            .srm_cm = PPA_SRM_COLOR_MODE_YUV422_UYVY,
            .yuv_range = PPA_COLOR_RANGE_LIMIT,
            .yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601,
        },
        .out = {
            .buffer = out,
            .buffer_size = s_buffer_size,
            .pic_w = PICTURE_SIZE,
            .pic_h = PICTURE_SIZE,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_270,
        .scale_x = (float)PICTURE_SIZE / (float)crop,
        .scale_y = (float)PICTURE_SIZE / (float)crop,
        .mirror_y = true,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    err = ppa_do_scale_rotate_mirror(s_ppa, &operation);
    if (err != ESP_OK) return err;
    return esp_cache_msync(out, s_buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}

static void close_camera(void)
{
    if (!s_camera) return;
    (void)mosaico_camera_stop_stream(s_camera);
    (void)mosaico_camera_close(s_camera);
    (void)mosaico_camera_del(s_camera);
    s_camera = NULL;
}

static esp_err_t open_camera(void)
{
    mosaico_camera_config_t config = MOSAICO_CAMERA_DEFAULT_CONFIG();
    config.allow_unidentified = true;       /* The module's identity chip may be blank. */
    esp_err_t err = mosaico_camera_new(&config, &s_camera);
    if (err == ESP_OK) err = mosaico_camera_open(s_camera);
    if (err == ESP_OK) err = mosaico_camera_start_stream(s_camera);
    if (err != ESP_OK && s_camera) {
        (void)mosaico_camera_del(s_camera);     /* Same clean-up as the camera preview example. */
        s_camera = NULL;
    }
    return err;
}

/* Capture until the screen closes or the camera fails. Returns the failure, or
 * ESP_OK when the screen closed. */
static esp_err_t run_stream(void)
{
    uint32_t failures = 0;
    bool restarted = false;
    uint8_t shown_zoom = 0;
    bool was_frozen = false;
    while (true) {
        const lens_request_t request = current_request();
        if (!request.active) return ESP_OK;
        /* Frozen: keep the last picture on show and leave the camera alone. */
        if (request.frozen) {
            was_frozen = true;
            vTaskDelay(pdMS_TO_TICKS(IDLE_POLL_MS));
            continue;
        }
        if (was_frozen) {       /* Frames queued while frozen are old; drop them. */
            (void)mosaico_camera_discard_frames(s_camera, FROZEN_DISCARD_FRAMES);
            was_frozen = false;
        }
        mosaico_camera_frame_t frame = {0};
        esp_err_t err = mosaico_camera_get_frame(s_camera, &frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "capture failed (%" PRIu32 "/%d): %s", ++failures, FAILURE_LIMIT, esp_err_to_name(err));
            if (failures >= FAILURE_LIMIT) {
                if (restarted) return err;
                err = mosaico_camera_restart(s_camera);
                if (err != ESP_OK) return err;
                restarted = true;
                failures = 0;
            }
            continue;
        }
        failures = 0;
        restarted = false;
        int back = pick_back_buffer();
        err = back == NO_BUFFER ? ESP_ERR_NO_MEM : convert(&frame, s_buffer[back], request.zoom);
        const esp_err_t returned = mosaico_camera_return_frame(s_camera, &frame);
        if (returned != ESP_OK) return returned;
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "convert failed: %s", esp_err_to_name(err));
            return err;
        }
        publish(back);
        if (shown_zoom != request.zoom) {
            ESP_LOGI(TAG, "zoom x%u", (unsigned)request.zoom);
            shown_zoom = request.zoom;
        }
    }
}

static void camera_task(void *arg)
{
    (void)arg;
    if (init_resources() != ESP_OK) {
        ESP_LOGE(TAG, "no memory for the lens; camera disabled");
        set_status(LENS_ERROR);
        vTaskDelete(NULL);
        return;
    }
    while (true) {
        if (!current_request().active) {
            close_camera();
            forget_picture();
            set_status(LENS_STARTING);
            vTaskDelay(pdMS_TO_TICKS(IDLE_POLL_MS));
            continue;
        }
        const esp_err_t opened = open_camera();
        if (opened != ESP_OK) {
            ESP_LOGW(TAG, "camera not available: %s", esp_err_to_name(opened));
            set_status(LENS_NO_CAMERA);
            vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
            continue;
        }
        ESP_LOGI(TAG, "camera started");
        const esp_err_t result = run_stream();
        close_camera();
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "camera stopped: %s", esp_err_to_name(result));
            forget_picture();
            set_status(LENS_ERROR);
            vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
        }
    }
}

bool lens_camera_start(void)
{
    return xTaskCreate(camera_task, "lens_cam", TASK_STACK, NULL, TASK_PRIORITY, NULL) == pdPASS;
}
