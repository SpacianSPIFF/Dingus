#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "packet.h"

esp_err_t espnow_recv_init(void);

// Copies the most recent command into *out. False if nothing has ever arrived.
bool espnow_recv_get_latest(turret_cmd_t *out);

// True if a packet arrived within the last 500 ms.
bool espnow_recv_link_alive(void);

// True exactly once per received trigger packet, even if newer packets arrived since.
bool espnow_recv_take_trigger(void);

// Sends telemetry back to the controller. Returns ESP_OK or ESP_ERR_INVALID_STATE if controller peer unknown yet.
esp_err_t espnow_send_telemetry(const turret_telemetry_t *telem);