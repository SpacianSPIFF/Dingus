#pragma once
#include <stdint.h>
#include "esp_err.h"

#define MPU6050_I2C_ADDR      0x68   // AD0 pin low (default, most breakout boards)
#define MPU6050_SDA_GPIO      21
#define MPU6050_SCL_GPIO      22
#define MPU6050_I2C_FREQ_HZ   400000

typedef struct {
    float accel_x, accel_y, accel_z;  // g
    float gyro_x, gyro_y, gyro_z;     // deg/s
} mpu6050_data_t;

esp_err_t mpu6050_init(void);
esp_err_t mpu6050_read(mpu6050_data_t *out);