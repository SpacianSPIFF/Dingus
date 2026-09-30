#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "packet.h"

esp_err_t espnow_recv_init(void);
bool espnow_recv_get_latest(turret_cmd_t *out);
bool espnow_recv_link_alive(void);