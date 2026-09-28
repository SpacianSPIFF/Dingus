#pragma once
#include "driver/i2c_master.h"

#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   32

esp_err_t oled_init(i2c_master_bus_handle_t bus);
void oled_clear(void);
void oled_draw_text(int x, int y, const char *text);  // y in 8px rows (0..3)
void oled_flush(void);