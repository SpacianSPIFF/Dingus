#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "imu_mpu6050.h"
#include "oled.h"
#include "buttons.h"
#include "gyro_cal.h"
#include "orientation.h"

typedef enum { MODE_ORIENT, MODE_MANUAL } controller_mode_t;

static void cal_progress_cb(int percent)
{
    oled_clear();
    oled_draw_text(0, 0, "CALIBRATING");
    oled_draw_text(0, 1, "HOLD STILL");
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", percent);
    oled_draw_text(0, 2, pct);
    oled_flush();
}

// Long-press detection for the mode button, layered on top of buttons.c's
// edge-triggered button_mode_pressed(). We track how long GPIO stays low
// ourselves here, separate from the debounced edge logic, since we need
// duration rather than just a press event.
#define LONG_PRESS_MS 1000

static bool mode_btn_was_down = false;
static int64_t mode_btn_down_since_us = 0;
static bool mode_long_press_fired = false;

// Returns MODE_ACTION_NONE, MODE_ACTION_TOGGLE (short press/release), or
// MODE_ACTION_ZERO_YAW (held past LONG_PRESS_MS).
typedef enum { MODE_ACTION_NONE, MODE_ACTION_TOGGLE, MODE_ACTION_ZERO_YAW } mode_action_t;

static mode_action_t poll_mode_button_action(void)
{
    bool level_low = gpio_get_level(BUTTON_MODE_GPIO) == 0;  // pressed = low
    int64_t now_us = esp_timer_get_time();

    if (level_low && !mode_btn_was_down) {
        // just pressed
        mode_btn_was_down = true;
        mode_btn_down_since_us = now_us;
        mode_long_press_fired = false;
    } else if (level_low && mode_btn_was_down) {
        // still held — check if we just crossed the long-press threshold
        if (!mode_long_press_fired &&
            (now_us - mode_btn_down_since_us) >= LONG_PRESS_MS * 1000) {
            mode_long_press_fired = true;
            return MODE_ACTION_ZERO_YAW;
        }
    } else if (!level_low && mode_btn_was_down) {
        // just released
        mode_btn_was_down = false;
        bool was_long = mode_long_press_fired;
        mode_long_press_fired = false;
        if (!was_long) {
            return MODE_ACTION_TOGGLE;  // short press/release = mode toggle
        }
        // else: was a long press, already handled on the threshold crossing, do nothing on release
    }

    return MODE_ACTION_NONE;
}

void app_main(void)
{
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_bus_init(&bus));
    ESP_ERROR_CHECK(mpu6050_init(bus));
    ESP_ERROR_CHECK(oled_init(bus));
    buttons_init();

    gyro_bias_t bias = {0};
    ESP_ERROR_CHECK(gyro_calibrate(&bias, cal_progress_cb));

    oled_clear();
    oled_draw_text(0, 0, "CAL DONE");
    oled_flush();
    vTaskDelay(pdMS_TO_TICKS(500));

    orientation_t orient;
    orientation_init(&orient);

    controller_mode_t mode = MODE_ORIENT;
    mpu6050_data_t imu;

    while (1) {
        mode_action_t action = poll_mode_button_action();
        if (action == MODE_ACTION_TOGGLE) {
            mode = (mode == MODE_ORIENT) ? MODE_MANUAL : MODE_ORIENT;
        } else if (action == MODE_ACTION_ZERO_YAW) {
            orientation_zero_yaw(&orient);
        }

        bool fired = button_fire_pressed();  // will feed into packet.trigger later

        mpu6050_read(&imu);

        // bias-corrected gyro values
        float gx = imu.gyro_x - bias.gyro_x;
        float gy = imu.gyro_y - bias.gyro_y;
        float gz = imu.gyro_z - bias.gyro_z;

        orientation_update(&orient, gx, gy, gz, imu.accel_x, imu.accel_y, imu.accel_z);

        oled_clear();
        oled_draw_text(0, 0, mode == MODE_ORIENT ? "MODE ORIENT" : "MODE MANUAL");
        char line[24];
        snprintf(line, sizeof(line), "R%d P%d Y%d",
                 (int)orient.roll_deg, (int)orient.pitch_deg, (int)orient.yaw_deg);
        oled_draw_text(0, 1, line);
        oled_draw_text(0, 2, fired ? "FIRE!" : "");
        oled_flush();

        vTaskDelay(pdMS_TO_TICKS(20));  // ~50Hz loop — faster than before, orientation math wants a tighter dt
    }
}