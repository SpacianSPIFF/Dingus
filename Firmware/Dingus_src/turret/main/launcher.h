#pragma once
#include "esp_err.h"

// Sets up both ESCs and the pusher, then blocks for ESC_ARM_MS while the ESCs arm.
// servo_timer_init() must have been called first.
esp_err_t launcher_init(void);

// Queue one shot. Spins the flywheels up if needed and pushes a dart once they're at speed.
// Ignored if a shot is already queued or in progress.
void launcher_request_fire(void);

// Call every loop iteration. Non-blocking.
void launcher_update(void);

// Link lost / emergency: cancel any queued shot, cut flywheels, retract the pusher.
void launcher_safe_stop(void);