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
#include "espnow_send.h"
#include "espnow_common.h"
#include "packet.h"

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

typedef enum { MODE_ACTION_NONE, MODE_ACTION_TOGGLE, MODE_ACTION_ZERO_YAW } mode_action_t;

static mode_action_t poll_mode_button_action(void)
{
    bool level_low = gpio_get_level(BUTTON_MODE_GPIO) == 0;  // pressed = low
    int64_t now_us = esp_timer_get_time();

    if (level_low && !mode_btn_was_down) {
        mode_btn_was_down = true;
        mode_btn_down_since_us = now_us;
        mode_long_press_fired = false;
    } else if (level_low && mode_btn_was_down) {
        if (!mode_long_press_fired &&
            (now_us - mode_btn_down_since_us) >= LONG_PRESS_MS * 1000) {
            mode_long_press_fired = true;
            return MODE_ACTION_ZERO_YAW;
        }
    } else if (!level_low && mode_btn_was_down) {
        mode_btn_was_down = false;
        bool was_long = mode_long_press_fired;
        mode_long_press_fired = false;
        if (!was_long) {
            return MODE_ACTION_TOGGLE;
        }
    }

    return MODE_ACTION_NONE;
}

void app_main(void)
{
    ESP_ERROR_CHECK(espnow_common_init());

    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_bus_init(&bus));
    ESP_ERROR_CHECK(mpu6050_init(bus));
    ESP_ERROR_CHECK(oled_init(bus));
    buttons_init();
    ESP_ERROR_CHECK(espnow_send_init());

    gyro_bias_t bias = {0};
    ESP_ERROR_CHECK(gyro_calibrate(&bias, cal_progress_cb));

    oled_clear();
    oled_draw_text(0, 0, "CAL DONE");
    oled_flush();
    vTaskDelay(pdMS_TO_TICKS(500));

    int64_t fire_display_until_us = 0;
    #define FIRE_DISPLAY_HOLD_MS 500

    orientation_t orient;
    orientation_init(&orient);

    controller_mode_t mode = MODE_ORIENT;
    mpu6050_data_t imu;
    uint32_t seq = 0;

    while (1) {
        mode_action_t action = poll_mode_button_action();
        if (action == MODE_ACTION_TOGGLE) {
            mode = (mode == MODE_ORIENT) ? MODE_MANUAL : MODE_ORIENT;
        } else if (action == MODE_ACTION_ZERO_YAW) {
            orientation_zero_yaw(&orient);
        }

        bool fired = button_fire_pressed();
        if (fired) {
            fire_display_until_us = esp_timer_get_time() + (FIRE_DISPLAY_HOLD_MS * 1000);
        }
        bool show_fire_text = esp_timer_get_time() < fire_display_until_us;

        mpu6050_read(&imu);
        float gx = imu.gyro_x - bias.gyro_x;
        float gy = imu.gyro_y - bias.gyro_y;
        float gz = imu.gyro_z - bias.gyro_z;
        orientation_update(&orient, gx, gy, gz, imu.accel_x, imu.accel_y, imu.accel_z);

        // Workspace clamping (±90 yaw, 0-60 pitch), deadband, and slew-rate limiting
        // are NOT applied yet — will be added once raw transmission is confirmed.
        turret_cmd_t cmd = {
            .seq = seq++,
            .yaw_deg = orient.yaw_deg,
            .pitch_deg = orient.pitch_deg,
            .trigger = fired ? 1 : 0,
        };
        espnow_send_cmd(&cmd);

        oled_clear();
        oled_draw_text(0, 0, mode == MODE_ORIENT ? "MODE ORIENT" : "MODE MANUAL");
        char line[24];
        snprintf(line, sizeof(line), "R%d P%d Y%d",
                 (int)orient.roll_deg, (int)orient.pitch_deg, (int)orient.yaw_deg);
        oled_draw_text(0, 1, line);
        oled_draw_text(0, 2, show_fire_text ? "*** FIRE ***" : "");
        oled_flush();

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}