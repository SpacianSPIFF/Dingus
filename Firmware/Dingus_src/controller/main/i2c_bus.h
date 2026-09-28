#pragma once
#include "driver/i2c_master.h"

#define I2C_BUS_SDA_GPIO   21
#define I2C_BUS_SCL_GPIO   22
#define I2C_BUS_FREQ_HZ    400000

esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_bus);