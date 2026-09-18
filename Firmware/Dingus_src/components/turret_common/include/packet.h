#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t seq;        // incrementing packet counter — turret uses arrival of new seq for failsafe/staleness check
    float    yaw_deg;    // commanded yaw angle, degrees, workspace-limited e.g. -90..+90
    float    pitch_deg;  // commanded pitch angle, degrees, workspace-limited e.g. 0..60
    uint8_t  trigger;    // edge-triggered single-shot: 1 only on the packet sent at the moment of button press, 0 otherwise
} turret_cmd_t;