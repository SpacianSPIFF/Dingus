#pragma once
#include "esp_err.h"

// Initializes the pusher servo and drives it to the retracted position.
esp_err_t pusher_init(void);
void pusher_extend(void);
void pusher_retract(void);