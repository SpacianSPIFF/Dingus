#pragma once
#include "esp_err.h"
#include "packet.h"

#define TURRET_MAC {0x80, 0xF3, 0xDA, 0x54, 0x61, 0x84}

esp_err_t espnow_send_init(void);
esp_err_t espnow_send_cmd(const turret_cmd_t *cmd);