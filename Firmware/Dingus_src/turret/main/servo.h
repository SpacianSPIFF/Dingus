#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/ledc.h"

// Standard hobby servo pulse range — verified against RDS5160 and MG946R datasheets.
// Trim per-servo if real hardware buzzes/strains at either extreme.
#define SERVO_PULSE_MIN_US 500
#define SERVO_PULSE_MAX_US 2500
#define SERVO_ANGLE_MIN    0
#define SERVO_ANGLE_MAX    180

#define SERVO_PWM_FREQ_HZ     50
#define SERVO_PWM_RESOLUTION  LEDC_TIMER_14_BIT  // 16384 steps — plenty of resolution at 50Hz

typedef struct {
    ledc_channel_t channel;
    int gpio;
} servo_t;

// Initializes the shared LEDC timer (call once) and configures one servo's channel.
esp_err_t servo_timer_init(void);
esp_err_t servo_init(servo_t *servo, ledc_channel_t channel, int gpio);

// angle_deg: 0-180, clamped internally to SERVO_ANGLE_MIN/MAX.
void servo_set_angle(const servo_t *servo, float angle_deg);