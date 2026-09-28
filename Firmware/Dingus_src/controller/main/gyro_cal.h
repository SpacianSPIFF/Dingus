#pragma once
#include "imu_mpu6050.h"

typedef struct {
    float gyro_x, gyro_y, gyro_z;  // deg/s, subtract from raw gyro reads
} gyro_bias_t;

// Blocks for the calibration window, sampling the gyro repeatedly.
// Controller must be held stationary throughout. Updates the OLED
// via the provided callback so the user knows to hold still.
typedef void (*gyro_cal_progress_cb_t)(int percent);

esp_err_t gyro_calibrate(gyro_bias_t *out_bias, gyro_cal_progress_cb_t progress_cb);