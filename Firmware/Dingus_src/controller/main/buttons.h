#pragma once
#include <stdbool.h>

#define BUTTON_FIRE_GPIO 4
#define BUTTON_MODE_GPIO 5

void buttons_init(void);

// Returns true exactly once per physical press (edge-triggered, debounced).
// Call once per main loop iteration for each button.
bool button_fire_pressed(void);
bool button_mode_pressed(void);