#include "espnow_send.h"
#include "esp_now.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "espnow_send";
static const uint8_t turret_mac[6] = TURRET_MAC;

static void send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "send failed");
    }
}

esp_err_t espnow_send_init(void)
{
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, turret_mac, 6);
    peer.channel = 0;   // use current WiFi channel
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "add_peer failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_now_register_send_cb(send_cb);
    ESP_LOGI(TAG, "ESP-NOW sender ready, target %02X:%02X:%02X:%02X:%02X:%02X",
              turret_mac[0], turret_mac[1], turret_mac[2],
              turret_mac[3], turret_mac[4], turret_mac[5]);
    return ESP_OK;
}

esp_err_t espnow_send_cmd(const turret_cmd_t *cmd)
{
    return esp_now_send(turret_mac, (const uint8_t *)cmd, sizeof(turret_cmd_t));
}