#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "espnow_common.h"
#include "espnow_recv.h"
#include "packet.h"
#include "servo.h"

// Limits for the angles
#define YAW_MIN_DEG    -90.0f
#define YAW_MAX_DEG     90.0f
#define PITCH_MIN_DEG   0.0f
#define PITCH_MAX_DEG   60.0f

// Deadband: ignore commanded changes smaller than this (sensor noise floor)
#define DEADBAND_DEG    0.5f

// Slew rate: max degrees the commanded angle may change per loop iteration
#define SLEW_RATE_DEG_PER_LOOP  2.0f

#define YAW_GPIO    18
#define PITCH_GPIO  19

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Applies deadband + slew-rate limiting. current is updated in place.
static float slew_towards(float current, float target, float max_step)
{
    float diff = target - current;
    if (fabsf(diff) < DEADBAND_DEG) return current;  // inside deadband, hold
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
    ESP_ERROR_CHECK(servo_init(&yaw_servo, LEDC_CHANNEL_0, YAW_GPIO));
    ESP_ERROR_CHECK(servo_init(&pitch_servo, LEDC_CHANNEL_1, PITCH_GPIO));

    // Servo angle convention: 90 = center. Yaw workspace -90..90 maps to servo 0..180.
    // Pitch workspace 0..60 maps directly (already servo-relative).
    float current_yaw_servo_deg = 90.0f;    // center
    float current_pitch_servo_deg = 0.0f;   // level

    while (1) {
        turret_cmd_t cmd;
        bool have_cmd = espnow_recv_get_latest(&cmd);
        bool link_ok = espnow_recv_link_alive();

        if (have_cmd && link_ok) {
            // Clamp commanded angles to workspace
            float target_yaw = clampf(cmd.yaw_deg, YAW_MIN_DEG, YAW_MAX_DEG);
            float target_pitch = clampf(cmd.pitch_deg, PITCH_MIN_DEG, PITCH_MAX_DEG);

            // Convert to servo-relative degrees (yaw: -90..90 -> 0..180)
            float target_yaw_servo = target_yaw + 90.0f;
            float target_pitch_servo = target_pitch;

            current_yaw_servo_deg = slew_towards(current_yaw_servo_deg, target_yaw_servo, SLEW_RATE_DEG_PER_LOOP);
            current_pitch_servo_deg = slew_towards(current_pitch_servo_deg, target_pitch_servo, SLEW_RATE_DEG_PER_LOOP);

            servo_set_angle(&yaw_servo, current_yaw_servo_deg);
            servo_set_angle(&pitch_servo, current_pitch_servo_deg);

            // Fire handling: only allowed when link is fresh
            if (cmd.trigger) {
                printf("FIRE (seq=%lu)\n", (unsigned long)cmd.seq);
                // actual firing mechanism trigger goes here once that hardware exists
            }
        } else {
            // No fresh command / link dead: HOLD last position, do not move servos,
            // and do not fire. current_yaw_servo_deg / current_pitch_servo_deg are
            // simply not updated, so servos stay exactly where they were.
            if (!link_ok && have_cmd) {
                printf("LINK LOST - holding position\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // ~50Hz, matches controller's send rate
    }
}