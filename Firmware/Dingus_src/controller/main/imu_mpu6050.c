#include "imu_mpu6050.h"
#include "esp_log.h"

static const char *TAG = "mpu6050";
static i2c_master_dev_handle_t dev_handle;

#define REG_PWR_MGMT_1   0x6B
#define REG_ACCEL_CONFIG 0x1C
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_XOUT_H 0x3B

#define ACCEL_SCALE 16384.0f
#define GYRO_SCALE  131.0f

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev_handle, buf, sizeof(buf), -1);
}

esp_err_t mpu6050_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_config, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    err = write_reg(REG_PWR_MGMT_1, 0x00);  // wake up
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wake failed: %s", esp_err_to_name(err));
        return err;
    }

    write_reg(REG_ACCEL_CONFIG, 0x00);  // ±2g
    write_reg(REG_GYRO_CONFIG, 0x00);   // ±250 deg/s

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