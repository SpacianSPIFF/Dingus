#pragma once
#include "driver/i2c_master.h"

#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   32

#define OLED_LINK_ICON_X   112
#define OLED_LINK_ICON_ROW 3

typedef enum {
    OLED_LINK_OK,
    OLED_LINK_LOST,
    OLED_LINK_REGAINED,
} oled_link_status_t;

esp_err_t oled_init(i2c_master_bus_handle_t bus);
void oled_clear(void);
void oled_draw_text(int x, int y, const char *text);  // y in 8px rows (0..3)
void oled_draw_link_icon(int x, int y_row, oled_link_status_t status);
void oled_draw_link_status(oled_link_status_t status);
void oled_flush(void);