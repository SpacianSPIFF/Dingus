#pragma once
#include "imu_mpu6050.h"

typedef struct {
    float roll_deg;   // accel-corrected
    float pitch_deg;  // accel-corrected
    float yaw_deg;     // gyro-only, drifts — zeroable via orientation_zero_yaw()
} orientation_t;

void orientation_init(orientation_t *orient);

// Call every loop iteration with the latest bias-corrected gyro (deg/s) and raw accel (g).
void orientation_update(orientation_t *orient, float gyro_x, float gyro_y, float gyro_z,
                         float accel_x, float accel_y, float accel_z);

void orientation_zero_yaw(orientation_t *orient);