#pragma once
#include "esp_err.h"

// Common ESP-NOW bring-up: sets NVS, WiFi STA mode (required by ESP-NOW), and
// initializes the ESP-NOW stack itself. Call once at boot on both sides.
esp_err_t espnow_common_init(void);