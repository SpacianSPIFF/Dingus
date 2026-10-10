#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t seq;        // incrementing packet counter — turret uses arrival of new seq for failsafe/staleness check
    float    yaw_deg;    // commanded yaw angle, degrees, workspace-limited e.g. -90..+90
    float    pitch_deg;  // commanded pitch angle, degrees, workspace-limited e.g. 0..60
    uint8_t  trigger;    // edge-triggered single-shot: 1 only on the packet sent at the moment of button press, 0 otherwise
} turret_cmd_t;

typedef enum {
    LAUNCHER_STATE_IDLE = 0,     // Flywheels off, system resting
    LAUNCHER_STATE_SPOOLING = 1, // Flywheels spinning up to target RPM
    LAUNCHER_STATE_FIRING = 2,   // Dart pusher extending/retracting (launching)
    LAUNCHER_STATE_READY = 3,    // Flywheels hot at speed, instant fire ready
} launcher_state_t;

typedef struct __attribute__((packed)) {
    uint32_t seq;             // sequence number echo from command
    float    yaw_deg;         // current achieved turret yaw angle, degrees (-90..+90)
    float    pitch_deg;       // current achieved turret pitch angle, degrees (0..60)
    uint8_t  launcher_state;  // launcher_state_t (IDLE, SPOOLING, FIRING, READY)
    uint8_t  ready_sec;       // countdown seconds remaining before flywheels spin down (0..5)
    uint8_t  flags;           // telemetry flags (e.g. bit 0: link alive)
} turret_telemetry_t;