#pragma once
#include "esp_err.h"
#include "servo.h"

// An ESC takes the same 50 Hz PWM as a servo; pulse width is throttle instead of angle.
typedef servo_t esc_t;

// Configures the channel and immediately outputs minimum throttle (needed for arming).
esp_err_t esc_init(esc_t *esc, ledc_channel_t channel, int gpio);

// pct: 0..100, clamped to ESC_MAX_THROTTLE_PCT from turret_config.h.
void esc_set_throttle(const esc_t *esc, float pct);