#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/ledc.h"

// Standard hobby-servo pulse range (verified for RDS5160 and MG946R; MG90S assumed the same).
#define SERVO_PULSE_MIN_US 800
#define SERVO_PULSE_MAX_US 2200

#define SERVO_PWM_FREQ_HZ     50
#define SERVO_PWM_RESOLUTION  LEDC_TIMER_14_BIT

typedef struct {
    ledc_channel_t channel;
    int gpio;
    float min_pulse_us;
    float max_pulse_us;
    float range_deg;   // mechanical range mapped across min_pulse_us..max_pulse_us
} servo_t;

esp_err_t servo_timer_init(void);
esp_err_t servo_init(servo_t *servo, ledc_channel_t channel, int gpio,
                     float min_pulse_us, float max_pulse_us, float range_deg);

// angle_deg is clamped to 0..range_deg.
void servo_set_angle(const servo_t *servo, float angle_deg);

// Raw pulse width, used directly by the ESC driver.
void servo_set_pulse_us(const servo_t *servo, float pulse_us);