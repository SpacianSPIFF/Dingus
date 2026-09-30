#include "espnow_recv.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "espnow_recv";

#define FAILSAFE_TIMEOUT_MS 500

static turret_cmd_t latest_cmd;
static bool has_received_ever = false;
static int64_t last_recv_us = 0;

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(turret_cmd_t)) {
        ESP_LOGW(TAG, "unexpected packet size: %d (expected %d)", len, (int)sizeof(turret_cmd_t));
        return;
    }

    memcpy(&latest_cmd, data, sizeof(turret_cmd_t));
    last_recv_us = esp_timer_get_time();
    has_received_ever = true;
}

esp_err_t espnow_recv_init(void)
{
    esp_err_t err = esp_now_register_recv_cb(recv_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_recv_cb failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "ESP-NOW receiver ready");
    return ESP_OK;
}

bool espnow_recv_get_latest(turret_cmd_t *out)
{
    if (!has_received_ever) return false;
    memcpy(out, &latest_cmd, sizeof(turret_cmd_t));
    return true;
}

bool espnow_recv_link_alive(void)
{
    if (!has_received_ever) return false;
    int64_t now_us = esp_timer_get_time();
    return (now_us - last_recv_us) < (FAILSAFE_TIMEOUT_MS * 1000);
}