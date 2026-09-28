#pragma once
#include <stdint.h>
#include "driver/i2c_master.h"

#define MPU6050_I2C_ADDR 0x68

typedef struct {
    float accel_x, accel_y, accel_z;  // g
    float gyro_x, gyro_y, gyro_z;     // deg/s
} mpu6050_data_t;

esp_err_t mpu6050_init(i2c_master_bus_handle_t bus);
esp_err_t mpu6050_read(mpu6050_data_t *out);