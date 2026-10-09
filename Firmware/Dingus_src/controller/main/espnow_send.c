#include "espnow_send.h"
#include "esp_now.h"
#include "esp_log.h"
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "espnow_send";
static const uint8_t turret_mac[6] = TURRET_MAC;

static portMUX_TYPE send_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t last_ack_time_us = 0;
static bool ever_received_ack = false;

static void send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        portENTER_CRITICAL(&send_lock);
        last_ack_time_us = esp_timer_get_time();
        ever_received_ack = true;
        portEXIT_CRITICAL(&send_lock);
    } else {
        ESP_LOGD(TAG, "send failed / no ack");
    }
}

bool espnow_send_is_connected(void)
{
    portENTER_CRITICAL(&send_lock);
    int64_t last = last_ack_time_us;
    bool acked = ever_received_ack;
    portEXIT_CRITICAL(&send_lock);

    if (!acked) return false;
    // Considered alive if an ACK arrived within the last 600 ms (controller transmits at ~50 Hz / 20 ms)
    return (esp_timer_get_time() - last) < 600000;
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