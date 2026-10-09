#include "esc.h"
#include "turret_config.h"

esp_err_t esc_init(esc_t *esc, ledc_channel_t channel, int gpio)
{
    esp_err_t err = servo_init(esc, channel, gpio, ESC_PULSE_MIN_US, ESC_PULSE_MAX_US, 100.0f);
    if (err != ESP_OK) return err;
    servo_set_pulse_us(esc, ESC_PULSE_MIN_US);
    return ESP_OK;
}

void esc_set_throttle(const esc_t *esc, float pct)
{
    if (pct < 0) pct = 0;
    if (pct > ESC_MAX_THROTTLE_PCT) pct = ESC_MAX_THROTTLE_PCT;

    float pulse_us = ESC_PULSE_MIN_US + (pct / 100.0f) * (ESC_PULSE_MAX_US - ESC_PULSE_MIN_US);
    servo_set_pulse_us(esc, pulse_us);
}