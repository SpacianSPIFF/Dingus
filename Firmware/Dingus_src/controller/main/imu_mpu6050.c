#include "imu_mpu6050.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mpu6050";
static i2c_master_dev_handle_t dev_handle;
static i2c_master_bus_handle_t bus_handle;

// MPU6050 register map (only what we need for now)
#define REG_PWR_MGMT_1   0x6B
#define REG_ACCEL_CONFIG 0x1C
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_XOUT_H 0x3B

// Default full-scale ranges: accel ±2g, gyro ±250 deg/s
#define ACCEL_SCALE 16384.0f   // LSB/g for ±2g
#define GYRO_SCALE  131.0f     // LSB/(deg/s) for ±250 deg/s

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev_handle, buf, sizeof(buf), -1);
}

esp_err_t mpu6050_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = MPU6050_SDA_GPIO,
        .scl_io_num = MPU6050_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = MPU6050_I2C_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    // Wake up the MPU6050 — it starts in sleep mode by default
    err = write_reg(REG_PWR_MGMT_1, 0x00);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to wake MPU6050: %s", esp_err_to_name(err));
        return err;
    }

    // Explicitly set accel ±2g, gyro ±250 deg/s (both are power-on defaults, set anyway to be explicit)
    write_reg(REG_ACCEL_CONFIG, 0x00);
    write_reg(REG_GYRO_CONFIG, 0x00);

    ESP_LOGI(TAG, "MPU6050 initialized");
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_data_t *out)
{
    uint8_t reg = REG_ACCEL_XOUT_H;
    uint8_t raw[14];

    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg, 1, raw, sizeof(raw), -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
        return err;
    }

    int16_t ax = (raw[0] << 8) | raw[1];
    int16_t ay = (raw[2] << 8) | raw[3];
    int16_t az = (raw[4] << 8) | raw[5];
    // raw[6],[7] = temperature — skipped
    int16_t gx = (raw[8]  << 8) | raw[9];
    int16_t gy = (raw[10] << 8) | raw[11];
    int16_t gz = (raw[12] << 8) | raw[13];

    out->accel_x = ax / ACCEL_SCALE;
    out->accel_y = ay / ACCEL_SCALE;
    out->accel_z = az / ACCEL_SCALE;
    out->gyro_x  = gx / GYRO_SCALE;
    out->gyro_y  = gy / GYRO_SCALE;
    out->gyro_z  = gz / GYRO_SCALE;

    return ESP_OK;
}