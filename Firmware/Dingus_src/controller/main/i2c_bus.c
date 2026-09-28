#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_bus)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_BUS_SDA_GPIO,
        .scl_io_num = I2C_BUS_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, out_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
    }
    return err;
}