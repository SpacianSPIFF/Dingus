#include "launcher.h"
#include <stdbool.h>
#include "esc.h"
#include "pusher.h"
#include "turret_config.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "launcher";

typedef enum { P_RETRACTED, P_EXTENDING, P_RETRACTING } pusher_state_t;

static esc_t esc_left, esc_right;

static float flywheel_pct = 0.0f;          // what we're actually outputting
static float flywheel_target_pct = 0.0f;   // where we're ramping to
static int64_t at_speed_since_us = 0;      // 0 = not at speed

static pusher_state_t pstate = P_RETRACTED;
static int64_t pusher_since_us = 0;
static bool shot_pending = false;

static int64_t last_update_us = 0;
static int64_t last_activity_us = 0;

static void apply_flywheels(void)
{
    esc_set_throttle(&esc_left, flywheel_pct);
    esc_set_throttle(&esc_right, flywheel_pct);
}

esp_err_t launcher_init(void)
{
    esp_err_t err = esc_init(&esc_left, CH_ESC_LEFT, PIN_ESC_LEFT);
    if (err != ESP_OK) return err;
    err = esc_init(&esc_right, CH_ESC_RIGHT, PIN_ESC_RIGHT);
    if (err != ESP_OK) return err;
    err = pusher_init();
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "arming ESCs: holding zero throttle for %d ms", ESC_ARM_MS);
    vTaskDelay(pdMS_TO_TICKS(ESC_ARM_MS));

    last_update_us = esp_timer_get_time();
    last_activity_us = last_update_us;
    ESP_LOGI(TAG, "launcher ready");
    return ESP_OK;
}

void launcher_request_fire(void)
{
    if (shot_pending || pstate != P_RETRACTED) {
        ESP_LOGW(TAG, "shot already in progress, ignoring");
        return;
    }
    shot_pending = true;
    flywheel_target_pct = FLYWHEEL_THROTTLE_PCT;
    last_activity_us = esp_timer_get_time();
    ESP_LOGI(TAG, "shot queued");
}

void launcher_safe_stop(void)
{
    shot_pending = false;
    flywheel_target_pct = 0.0f;
    flywheel_pct = 0.0f;          // cut immediately; the motors coast down
    at_speed_since_us = 0;
    apply_flywheels();

    pusher_retract();
    if (pstate == P_EXTENDING) {
        pstate = P_RETRACTING;
        pusher_since_us = esp_timer_get_time();
    }
    ESP_LOGW(TAG, "safe stop: flywheels cut, pusher retracting");
}

void launcher_update(void)
{
    int64_t now = esp_timer_get_time();
    float dt = (now - last_update_us) / 1000000.0f;
    last_update_us = now;
    if (dt < 0 || dt > 0.5f) dt = LOOP_PERIOD_MS / 1000.0f;

    // 1) Ramp flywheels toward the target
    float step = FLYWHEEL_RAMP_PCT_PER_SEC * dt;
    if (flywheel_pct < flywheel_target_pct) {
        flywheel_pct += step;
        if (flywheel_pct > flywheel_target_pct) flywheel_pct = flywheel_target_pct;
    } else if (flywheel_pct > flywheel_target_pct) {
        flywheel_pct -= step;
        if (flywheel_pct < flywheel_target_pct) flywheel_pct = flywheel_target_pct;
    }
    apply_flywheels();

    // 2) Track how long we've been at target speed
    bool at_speed = (flywheel_target_pct > 0.0f) && (flywheel_pct >= flywheel_target_pct);
    if (at_speed) {
        if (at_speed_since_us == 0) at_speed_since_us = now;
    } else {
        at_speed_since_us = 0;
    }
    bool ready = at_speed && (now - at_speed_since_us) >= (int64_t)FLYWHEEL_SETTLE_MS * 1000;

    // 3) Pusher sequence
    switch (pstate) {
    case P_RETRACTED:
        if (shot_pending && ready) {
            pusher_extend();
            pstate = P_EXTENDING;
            pusher_since_us = now;
            shot_pending = false;
            ESP_LOGI(TAG, "FIRE: pushing dart");
        }
        break;
    case P_EXTENDING:
        if (now - pusher_since_us >= (int64_t)PUSHER_EXTEND_MS * 1000) {
            pusher_retract();
            pstate = P_RETRACTING;
            pusher_since_us = now;
        }
        break;
    case P_RETRACTING:
        if (now - pusher_since_us >= (int64_t)PUSHER_RETRACT_MS * 1000) {
            pstate = P_RETRACTED;
            last_activity_us = now;
        }
        break;
    }

    // 4) Spin down after a period of no shots
    if (flywheel_target_pct > 0.0f && !shot_pending && pstate == P_RETRACTED &&
        (now - last_activity_us) >= (int64_t)FLYWHEEL_HOLD_MS * 1000) {
        flywheel_target_pct = 0.0f;
        ESP_LOGI(TAG, "idle timeout: spinning flywheels down");
    }
}

launcher_state_t launcher_get_state(uint8_t *out_countdown_sec)
{
    if (out_countdown_sec) *out_countdown_sec = 0;

    if (pstate != P_RETRACTED) {
        return LAUNCHER_STATE_FIRING;
    }
    if (shot_pending) {
        return LAUNCHER_STATE_SPOOLING;
    }
    if (flywheel_target_pct > 0.0f) {
        int64_t now = esp_timer_get_time();
        int64_t elapsed_us = now - last_activity_us;
        int64_t remaining_us = ((int64_t)FLYWHEEL_HOLD_MS * 1000) - elapsed_us;
        if (remaining_us > 0) {
            uint8_t sec = (uint8_t)((remaining_us + 999999) / 1000000);
            if (out_countdown_sec) *out_countdown_sec = sec;
            return LAUNCHER_STATE_READY;
        }
    }
    return LAUNCHER_STATE_IDLE;
}