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
#define BUTTON_DEBOUNCE_US 30000   // 30 ms debounce
#define LONG_PRESS_US      800000   // 800 ms hold to re-zero yaw

static bool btn_stable_state = false;        // true = pressed (low)
static bool btn_last_raw = false;
static int64_t btn_last_transition_us = 0;
static int64_t btn_press_start_us = 0;
static bool long_press_handled = false;

typedef enum { MODE_ACTION_NONE, MODE_ACTION_TOGGLE, MODE_ACTION_ZERO_YAW } mode_action_t;

static mode_action_t poll_mode_button_action(void)
{
    bool raw_pressed = (gpio_get_level(BUTTON_MODE_GPIO) == 0);
    int64_t now_us = esp_timer_get_time();

    // Track state change timestamp for debouncing
    if (raw_pressed != btn_last_raw) {
        btn_last_transition_us = now_us;
        btn_last_raw = raw_pressed;
    }

    // Only update stable state once input has stayed constant longer than debounce threshold
    if ((now_us - btn_last_transition_us) > BUTTON_DEBOUNCE_US) {
        if (raw_pressed != btn_stable_state) {
            btn_stable_state = raw_pressed;
            if (btn_stable_state) {
                // Button pressed down
                btn_press_start_us = now_us;
                long_press_handled = false;
            } else {
                // Button released
                if (!long_press_handled) {
                    return MODE_ACTION_TOGGLE;
                }
            }
        }
    }

    // Check for continuous hold while button remains depressed
    if (btn_stable_state && !long_press_handled) {
        if ((now_us - btn_press_start_us) >= LONG_PRESS_US) {
            long_press_handled = true;
            return MODE_ACTION_ZERO_YAW;
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
    int64_t yaw_zeroed_display_until_us = 0;
    int64_t reconnected_display_until_us = 0;
    bool link_was_ok = false;
    #define FIRE_DISPLAY_HOLD_MS 500

    orientation_t orient;
    orientation_init(&orient);

    controller_mode_t mode = MODE_ORIENT;
    mpu6050_data_t imu;
    uint32_t seq = 0;

    while (1) {
        int64_t now_us = esp_timer_get_time();

        mode_action_t action = poll_mode_button_action();
        if (action == MODE_ACTION_TOGGLE) {
            mode = (mode == MODE_ORIENT) ? MODE_MANUAL : MODE_ORIENT;
        } else if (action == MODE_ACTION_ZERO_YAW) {
            orientation_zero_yaw(&orient);
            yaw_zeroed_display_until_us = now_us + 1000000;  // Display "YAW ZEROED" for 1 sec
        }

        bool fired = button_fire_pressed();
        if (fired) {
            fire_display_until_us = now_us + (FIRE_DISPLAY_HOLD_MS * 1000);
        }
        bool show_fire_text = now_us < fire_display_until_us;

        mpu6050_read(&imu);
        float gx = imu.gyro_x - bias.gyro_x;
        float gy = imu.gyro_y - bias.gyro_y;
        float gz = imu.gyro_z - bias.gyro_z;
        orientation_update(&orient, gx, gy, gz, imu.accel_x, imu.accel_y, imu.accel_z);

        // MODE_ORIENT: uses Gyro Z integration (aim by rotating remote left/right)
        // MODE_MANUAL: uses Roll tilt (aim by tilting remote left/right, 100% drift-free)
        float commanded_yaw = (mode == MODE_ORIENT) ? orient.yaw_deg : orient.roll_deg;

        turret_cmd_t cmd = {
            .seq = seq++,
            .yaw_deg = commanded_yaw,
            .pitch_deg = orient.pitch_deg,
            .trigger = fired ? 1 : 0,
        };
        espnow_send_cmd(&cmd);

        // Check wireless link ACK status
        bool link_ok = espnow_send_is_connected();
        if (link_ok && !link_was_ok) {
            reconnected_display_until_us = now_us + 1500000;  // show for 1.5s
        }
        link_was_ok = link_ok;

        oled_clear();
        // Row 0: Mode description
        oled_draw_text(0, 0, mode == MODE_ORIENT ? "MODE: YAW (TURN)" : "MODE: TILT(ROLL)");

        // Row 1: Orientation angles
        char line[24];
        snprintf(line, sizeof(line), "R%3d P%3d Y%3d",
                 (int)orient.roll_deg, (int)orient.pitch_deg, (int)commanded_yaw);
        oled_draw_text(0, 1, line);

        // Row 2: Fire flash or Yaw Zero notification
        if (show_fire_text) {
            oled_draw_text(0, 2, "*** FIRE ***");
        } else if (now_us < yaw_zeroed_display_until_us) {
            oled_draw_text(0, 2, "YAW ZEROED!");
        }

        // Row 3: Link connection status
        if (!link_ok) {
            oled_draw_text(0, 3, "LINK: LOST!");
        } else if (now_us < reconnected_display_until_us) {
            oled_draw_text(0, 3, "LINK: REGAINED");
        } else {
            oled_draw_text(0, 3, "LINK: OK");
        }

        oled_flush();

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}