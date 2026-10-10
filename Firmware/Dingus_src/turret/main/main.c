#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "espnow_common.h"
#include "espnow_recv.h"
#include "packet.h"
#include "servo.h"
#include "launcher.h"
#include "turret_config.h"

static const char *TAG = "turret";

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Deadband + slew-rate limiting toward a target angle.
static float slew_towards(float current, float target, float max_step)
{
    float diff = target - current;
    if (fabsf(diff) < DEADBAND_DEG) return current;
    if (diff > max_step) diff = max_step;
    if (diff < -max_step) diff = -max_step;
    return current + diff;
}

void app_main(void)
{
    ESP_ERROR_CHECK(espnow_common_init());
    ESP_ERROR_CHECK(espnow_recv_init());

    ESP_ERROR_CHECK(servo_timer_init());
    servo_t yaw_servo, pitch_servo;
    ESP_ERROR_CHECK(servo_init(&yaw_servo, CH_YAW, PIN_YAW_SERVO,
                               YAW_SERVO_PULSE_MIN_US, YAW_SERVO_PULSE_MAX_US, YAW_SERVO_RANGE_DEG));
    ESP_ERROR_CHECK(servo_init(&pitch_servo, CH_PITCH, PIN_PITCH_SERVO,
                               PITCH_SERVO_PULSE_MIN_US, PITCH_SERVO_PULSE_MAX_US, PITCH_SERVO_RANGE_DEG));

    // Yaw 0 deg (controller convention) sits at the servo's mechanical center (90 deg on 180 deg MG946R).
    const float yaw_center = YAW_SERVO_RANGE_DEG / 2.0f;
    float current_yaw_servo_deg = yaw_center;
    float current_pitch_servo_deg = PITCH_ZERO_OFFSET_DEG;

    // Output a position right away so the servos hold from boot instead of going limp.
    servo_set_angle(&yaw_servo, current_yaw_servo_deg);
    servo_set_angle(&pitch_servo, current_pitch_servo_deg);

    // Blocks for ESC_ARM_MS while the ESCs arm. Keep flywheel wheels clear of fingers.
    ESP_ERROR_CHECK(launcher_init());

    bool link_was_ok = false;

    while (1) {
        turret_cmd_t cmd = {0};
        bool have_cmd = espnow_recv_get_latest(&cmd);
        bool link_ok = espnow_recv_link_alive();
        bool fire = espnow_recv_take_trigger();   // always consumed, so stale triggers can't pile up

        if (link_ok && !link_was_ok) {
            ESP_LOGI(TAG, "link up");
        } else if (!link_ok && link_was_ok) {
            ESP_LOGW(TAG, "LINK LOST - holding position, flywheels off");
            launcher_safe_stop();
        }
        link_was_ok = link_ok;

        if (have_cmd && link_ok) {
            float target_yaw = clampf(cmd.yaw_deg, YAW_MIN_DEG, YAW_MAX_DEG);
            float target_pitch = clampf(cmd.pitch_deg, PITCH_MIN_DEG, PITCH_MAX_DEG);

            float target_yaw_servo = target_yaw + yaw_center;
            float target_pitch_servo = target_pitch + PITCH_ZERO_OFFSET_DEG;

            current_yaw_servo_deg = slew_towards(current_yaw_servo_deg, target_yaw_servo, SLEW_RATE_DEG_PER_LOOP);
            current_pitch_servo_deg = slew_towards(current_pitch_servo_deg, target_pitch_servo, SLEW_RATE_DEG_PER_LOOP);

            servo_set_angle(&yaw_servo, current_yaw_servo_deg);
            servo_set_angle(&pitch_servo, current_pitch_servo_deg);

            if (fire) launcher_request_fire();
        } else if (fire) {
            ESP_LOGW(TAG, "fire ignored: link not alive");
        }
        // Link dead: servos simply aren't updated, so they hold their last position.

        launcher_update();

        // Query launcher state & countdown
        uint8_t ready_sec = 0;
        launcher_state_t l_state = launcher_get_state(&ready_sec);

        // Send achieved workspace angles and launcher state back to handheld controller
        float live_yaw = current_yaw_servo_deg - yaw_center;
        float live_pitch = current_pitch_servo_deg - PITCH_ZERO_OFFSET_DEG;
        turret_telemetry_t telem = {
            .seq = cmd.seq,
            .yaw_deg = live_yaw,
            .pitch_deg = live_pitch,
            .launcher_state = (uint8_t)l_state,
            .ready_sec = ready_sec,
            .flags = (link_ok ? 1 : 0),
        };
        espnow_send_telemetry(&telem);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}