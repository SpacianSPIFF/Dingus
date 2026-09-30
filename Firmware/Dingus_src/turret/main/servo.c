#include "servo.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "servo";

esp_err_t servo_timer_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer_config failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t servo_init(servo_t *servo, ledc_channel_t channel, int gpio)
{
    servo->channel = channel;
    servo->gpio = gpio;

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio,
        .duty = 0,
        .hpoint = 0,
    };
    esp_err_t err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "channel_config failed on GPIO%d: %s", gpio, esp_err_to_name(err));
    }
    return err;
}

void servo_set_angle(const servo_t *servo, float angle_deg)
{
    if (angle_deg < SERVO_ANGLE_MIN) angle_deg = SERVO_ANGLE_MIN;
    if (angle_deg > SERVO_ANGLE_MAX) angle_deg = SERVO_ANGLE_MAX;

    // Map angle -> pulse width (us) -> duty cycle counts
    float pulse_us = SERVO_PULSE_MIN_US +
        (angle_deg / SERVO_ANGLE_MAX) * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US);

    uint32_t max_duty = (1 << SERVO_PWM_RESOLUTION) - 1;
    float period_us = 1000000.0f / SERVO_PWM_FREQ_HZ;  // 20000us at 50Hz
    uint32_t duty = (uint32_t)((pulse_us / period_us) * max_duty);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel);
}