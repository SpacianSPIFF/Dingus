#include "buttons.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define DEBOUNCE_US 30000  // 30ms

typedef struct {
    int gpio;
    bool prev_level;       // previous raw level (true = released/high)
    int64_t last_change_us;
} button_state_t;

static button_state_t fire_btn = { .gpio = BUTTON_FIRE_GPIO, .prev_level = true };
static button_state_t mode_btn = { .gpio = BUTTON_MODE_GPIO, .prev_level = true };

static void init_one(int gpio)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

void buttons_init(void)
{
    init_one(BUTTON_FIRE_GPIO);
    init_one(BUTTON_MODE_GPIO);
}

// Returns true on a debounced high->low transition (button pressed, pulls to GND).
static bool poll_edge(button_state_t *btn)
{
    bool level = gpio_get_level(btn->gpio) != 0;  // true = high = not pressed
    int64_t now = esp_timer_get_time();

    if (level != btn->prev_level) {
        if (now - btn->last_change_us < DEBOUNCE_US) {
            return false;  // bounce, ignore
        }
        btn->last_change_us = now;
        bool was_pressed_edge = (btn->prev_level == true && level == false);
        btn->prev_level = level;
        return was_pressed_edge;
    }
    return false;
}

bool button_fire_pressed(void) { return poll_edge(&fire_btn); }
bool button_mode_pressed(void) { return poll_edge(&mode_btn); }