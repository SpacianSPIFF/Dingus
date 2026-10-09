#include "pusher.h"
#include "servo.h"
#include "turret_config.h"

static servo_t pusher_servo;

esp_err_t pusher_init(void)
{
    esp_err_t err = servo_init(&pusher_servo, CH_PUSHER, PIN_PUSHER_SERVO,
                               PUSHER_PULSE_MIN_US, PUSHER_PULSE_MAX_US, PUSHER_SERVO_RANGE_DEG);
    if (err != ESP_OK) return err;
    servo_set_angle(&pusher_servo, PUSHER_RETRACT_DEG);
    return ESP_OK;
}

void pusher_extend(void)  { servo_set_angle(&pusher_servo, PUSHER_EXTEND_DEG); }
void pusher_retract(void) { servo_set_angle(&pusher_servo, PUSHER_RETRACT_DEG); }