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

typedef enum {
    MODE_ORIENT,   // Both Yaw and Pitch active simultaneously
    MODE_YAW,      // Only Yaw active (Pitch locked)
    MODE_PITCH     // Only Pitch active (Yaw locked)
} controller_mode_t;

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
    float locked_pitch_deg = 0.0f;
    float locked_yaw_deg = 0.0f;
    mpu6050_data_t imu;
    uint32_t seq = 0;

    while (1) {
        int64_t now_us = esp_timer_get_time();

        mode_action_t action = poll_mode_button_action();
        if (action == MODE_ACTION_TOGGLE) {
            // Cycle: ORIENT (both) -> YAW ONLY -> PITCH ONLY -> ORIENT
            if (mode == MODE_ORIENT) {
                mode = MODE_YAW;
                locked_pitch_deg = orient.pitch_deg;
            } else if (mode == MODE_YAW) {
                mode = MODE_PITCH;
                locked_yaw_deg = orient.yaw_deg;
            } else {
                mode = MODE_ORIENT;
            }
        } else if (action == MODE_ACTION_ZERO_YAW) {
            orientation_zero_yaw(&orient);
            locked_yaw_deg = 0.0f;
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

        // Determine commanded angles based on active mode
        float commanded_yaw = orient.yaw_deg;
        float commanded_pitch = orient.pitch_deg;
        switch (mode) {
        case MODE_ORIENT:
            commanded_yaw = orient.yaw_deg;
            commanded_pitch = orient.pitch_deg;
            break;
        case MODE_YAW:
            commanded_yaw = orient.yaw_deg;
            commanded_pitch = locked_pitch_deg;
            break;
        case MODE_PITCH:
            commanded_yaw = locked_yaw_deg;
            commanded_pitch = orient.pitch_deg;
            break;
        default:
            commanded_yaw = orient.yaw_deg;
            commanded_pitch = orient.pitch_deg;
            break;
        }

        turret_cmd_t cmd = {
            .seq = seq++,
            .yaw_deg = commanded_yaw,
            .pitch_deg = commanded_pitch,
            .trigger = fired ? 1 : 0,
        };
        espnow_send_cmd(&cmd);

        // Check wireless link ACK status
        bool link_ok = espnow_send_is_connected();
        if (link_ok && !link_was_ok) {
            reconnected_display_until_us = now_us + 1500000;  // show for 1.5s
        }
        link_was_ok = link_ok;

        // Query live telemetry received from the turret
        turret_telemetry_t telem = {0};
        bool have_telem = espnow_get_telemetry(&telem);

        oled_clear();
        // Row 0: Mode description
        switch (mode) {
        case MODE_ORIENT:
            oled_draw_text(0, 0, "MODE: ORIENT (Y+P)");
            break;
        case MODE_YAW:
            oled_draw_text(0, 0, "MODE: YAW ONLY");
            break;
        case MODE_PITCH:
            oled_draw_text(0, 0, "MODE: PITCH ONLY");
            break;
        default:
            oled_draw_text(0, 0, "MODE: ORIENT (Y+P)");
            break;
        }

        // Row 1: Target/Commanded angles from remote
        char line[24];
        snprintf(line, sizeof(line), "CMD: P%3d  Y%3d",
                 (int)commanded_pitch, (int)commanded_yaw);
        oled_draw_text(0, 1, line);

        // Row 2: Live actual turret angles from telemetry (always visible)
        if (have_telem) {
            snprintf(line, sizeof(line), "TUR: P%3d  Y%3d",
                     (int)telem.pitch_deg, (int)telem.yaw_deg);
            oled_draw_text(0, 2, line);
        } else {
            oled_draw_text(0, 2, "TUR: NO DATA");
        }

        // Row 3: Launcher state & action notifications on the left
        if (now_us < yaw_zeroed_display_until_us) {
            oled_draw_text(0, 3, "YAW ZEROED!");
        } else if (!link_ok) {
            oled_draw_text(0, 3, "LINK LOST");
        } else if (have_telem) {
            switch (telem.launcher_state) {
            case LAUNCHER_STATE_FIRING:
                oled_draw_text(0, 3, "*** FIRING! ***");
                break;
            case LAUNCHER_STATE_SPOOLING: {
                int dot_count = (int)((now_us / 250000) % 4);
                char sbuf[16];
                if (dot_count == 0)      snprintf(sbuf, sizeof(sbuf), "SPOOLING    ");
                else if (dot_count == 1) snprintf(sbuf, sizeof(sbuf), "SPOOLING .  ");
                else if (dot_count == 2) snprintf(sbuf, sizeof(sbuf), "SPOOLING .. ");
                else                     snprintf(sbuf, sizeof(sbuf), "SPOOLING ...");
                oled_draw_text(0, 3, sbuf);
                break;
            }
            case LAUNCHER_STATE_READY: {
                char lbuf[20];
                snprintf(lbuf, sizeof(lbuf), "HOT: READY (%ds)", telem.ready_sec);
                oled_draw_text(0, 3, lbuf);
                break;
            }
            case LAUNCHER_STATE_IDLE:
            default:
                if (show_fire_text) {
                    oled_draw_text(0, 3, "TRIGGER SENT");
                }
                break;
            }
        } else if (show_fire_text) {
            oled_draw_text(0, 3, "TRIGGER SENT");
        }

        // Bottom right: Wireless link status icon (Live Wi-Fi, Slashed Wi-Fi, or Regained checkmark)
        oled_link_status_t link_status = OLED_LINK_OK;
        if (!link_ok) {
            link_status = OLED_LINK_LOST;
        } else if (now_us < reconnected_display_until_us) {
            link_status = OLED_LINK_REGAINED;
        }
        oled_draw_link_status(link_status);

        oled_flush();

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}