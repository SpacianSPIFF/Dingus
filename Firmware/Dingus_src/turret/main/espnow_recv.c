#include "espnow_recv.h"
#include <string.h>
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "espnow_recv";

#define FAILSAFE_TIMEOUT_MS 500

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static turret_cmd_t latest_cmd;
static bool has_received_ever = false;
static int64_t last_recv_us = 0;
static bool trigger_pending = false;
static uint8_t controller_mac[6] = {0};
static bool has_controller_mac = false;

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(turret_cmd_t)) {
        ESP_LOGW(TAG, "unexpected packet size: %d (expected %d)", len, (int)sizeof(turret_cmd_t));
        return;
    }

    turret_cmd_t tmp;
    memcpy(&tmp, data, sizeof(tmp));
    int64_t now_us = esp_timer_get_time();

    portENTER_CRITICAL(&lock);
    if (!has_controller_mac && info && info->src_addr) {
        memcpy(controller_mac, info->src_addr, 6);
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, controller_mac, 6);
        peer.channel = 0;
        peer.encrypt = false;
        esp_err_t p_err = esp_now_add_peer(&peer);
        if (p_err == ESP_OK || p_err == ESP_ERR_ESPNOW_EXIST) {
            has_controller_mac = true;
        }
    }

    bool duplicate = has_received_ever && (tmp.seq == latest_cmd.seq);
    if (!duplicate) {
        latest_cmd = tmp;
        has_received_ever = true;
        if (tmp.trigger) trigger_pending = true;   // latched until main takes it
    }
    last_recv_us = now_us;
    portEXIT_CRITICAL(&lock);
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
    bool have;
    portENTER_CRITICAL(&lock);
    have = has_received_ever;
    if (have) memcpy(out, &latest_cmd, sizeof(turret_cmd_t));
    portEXIT_CRITICAL(&lock);
    return have;
}

bool espnow_recv_link_alive(void)
{
    bool have;
    int64_t last;
    portENTER_CRITICAL(&lock);
    have = has_received_ever;
    last = last_recv_us;
    portEXIT_CRITICAL(&lock);

    if (!have) return false;
    return (esp_timer_get_time() - last) < ((int64_t)FAILSAFE_TIMEOUT_MS * 1000);
}

bool espnow_recv_take_trigger(void)
{
    bool t;
    portENTER_CRITICAL(&lock);
    t = trigger_pending;
    trigger_pending = false;
    portEXIT_CRITICAL(&lock);
    return t;
}

esp_err_t espnow_send_telemetry(const turret_telemetry_t *telem)
{
    uint8_t mac[6];
    bool have_peer;
    portENTER_CRITICAL(&lock);
    have_peer = has_controller_mac;
    if (have_peer) memcpy(mac, controller_mac, 6);
    portEXIT_CRITICAL(&lock);

    if (!have_peer) return ESP_ERR_INVALID_STATE;
    return esp_now_send(mac, (const uint8_t *)telem, sizeof(turret_telemetry_t));
}