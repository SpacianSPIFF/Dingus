#include "gyro_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "gyro_cal";

#define CAL_DURATION_MS   3000
#define CAL_SAMPLE_MS     10   // ~100Hz sampling during cal

esp_err_t gyro_calibrate(gyro_bias_t *out_bias, gyro_cal_progress_cb_t progress_cb)
{
    double sum_x = 0, sum_y = 0, sum_z = 0;
    int samples = 0;
    int total_samples = CAL_DURATION_MS / CAL_SAMPLE_MS;

    ESP_LOGI(TAG, "starting gyro calibration, hold still for %d ms", CAL_DURATION_MS);

    for (int i = 0; i < total_samples; i++) {
        mpu6050_data_t data;
        esp_err_t err = mpu6050_read(&data);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read failed during calibration: %s", esp_err_to_name(err));
            return err;
        }

        sum_x += data.gyro_x;
        sum_y += data.gyro_y;
        sum_z += data.gyro_z;
        samples++;

        if (progress_cb) {
            int percent = (i * 100) / total_samples;
            progress_cb(percent);
        }

        vTaskDelay(pdMS_TO_TICKS(CAL_SAMPLE_MS));
    }

    out_bias->gyro_x = (float)(sum_x / samples);
    out_bias->gyro_y = (float)(sum_y / samples);
    out_bias->gyro_z = (float)(sum_z / samples);

    ESP_LOGI(TAG, "calibration done: bias x=%.3f y=%.3f z=%.3f",
              out_bias->gyro_x, out_bias->gyro_y, out_bias->gyro_z);

    if (progress_cb) progress_cb(100);

    return ESP_OK;
}