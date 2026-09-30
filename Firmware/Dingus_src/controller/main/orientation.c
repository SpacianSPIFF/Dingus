#include "orientation.h"
#include "esp_timer.h"
#include <math.h>

#define ALPHA 0.98f  // weight given to gyro-integrated angle each step

static int64_t last_update_us = 0;

void orientation_init(orientation_t *orient)
{
    orient->roll_deg = 0;
    orient->pitch_deg = 0;
    orient->yaw_deg = 0;
    last_update_us = esp_timer_get_time();
}

void orientation_update(orientation_t *orient, float gyro_x, float gyro_y, float gyro_z,
                         float accel_x, float accel_y, float accel_z)
{
    int64_t now_us = esp_timer_get_time();
    float dt = (now_us - last_update_us) / 1000000.0f;
    last_update_us = now_us;

    // Guard against a stray huge dt (e.g. first call, or a stall) corrupting the integral
    if (dt <= 0 || dt > 0.5f) dt = 0.01f;

    // Accelerometer-derived roll/pitch — valid only when accel is close to 1g
    // (i.e. mostly gravity, not much linear acceleration). atan2 avoids
    // divide-by-zero and gives the correct quadrant.
    float accel_roll  = atan2f(accel_x, accel_z) * 180.0f / (float)M_PI;
    float accel_pitch = atan2f(-accel_y, sqrtf(accel_x * accel_x + accel_z * accel_z)) * 180.0f / (float)M_PI;

    // Gyro-integrated angle for this step
    float gyro_roll  = orient->roll_deg  + gyro_y * dt;
    float gyro_pitch = orient->pitch_deg + gyro_x * dt;

    // Blend: mostly gyro, corrected toward accel
    orient->roll_deg  = ALPHA * gyro_roll  + (1.0f - ALPHA) * accel_roll;
    orient->pitch_deg = ALPHA * gyro_pitch + (1.0f - ALPHA) * accel_pitch;

    // Yaw: no correction source, pure integration — will drift
    orient->yaw_deg += gyro_z * dt;
}

void orientation_zero_yaw(orientation_t *orient)
{
    orient->yaw_deg = 0;
}