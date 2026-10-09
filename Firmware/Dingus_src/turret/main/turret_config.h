#pragma once
#include "driver/ledc.h"

/* ---------- GPIO assignments ---------- */
#define PIN_YAW_SERVO      18
#define PIN_PITCH_SERVO    19
#define PIN_PUSHER_SERVO   23
#define PIN_ESC_LEFT       25
#define PIN_ESC_RIGHT      26

/* ---------- LEDC channels (all share timer 0 at 50 Hz) ---------- */
#define CH_YAW        LEDC_CHANNEL_0
#define CH_PITCH      LEDC_CHANNEL_1
#define CH_PUSHER     LEDC_CHANNEL_2
#define CH_ESC_LEFT   LEDC_CHANNEL_3
#define CH_ESC_RIGHT  LEDC_CHANNEL_4

/* ---------- Aiming Servos ---------- */
#define YAW_MIN_DEG      -90.0f
#define YAW_MAX_DEG       90.0f
#define PITCH_MIN_DEG      0.0f
#define PITCH_MAX_DEG     60.0f

// YAW: MG946R metal gear servo (180 deg mechanical range, standard 500-2500 us pulse)
#define YAW_SERVO_RANGE_DEG       180.0f
#define YAW_SERVO_PULSE_MIN_US    500.0f
#define YAW_SERVO_PULSE_MAX_US    2500.0f

// PITCH: RDS5160 robot servo (270 deg mechanical range across 500-2500 us pulse)
#define PITCH_SERVO_RANGE_DEG     270.0f
#define PITCH_SERVO_PULSE_MIN_US  500.0f
#define PITCH_SERVO_PULSE_MAX_US  2500.0f
#define PITCH_ZERO_OFFSET_DEG     0.0f

#define DEADBAND_DEG             0.5f   // ignore commanded changes smaller than this
#define SLEW_RATE_DEG_PER_LOOP   4.0f   // max change per loop iteration (4 deg/step = 200 deg/s for snappy tracking)
#define LOOP_PERIOD_MS           20     // ~50 Hz

/* ---------- Flywheels (ESCs: SimonK with RS2212 920KV) ---------- */
#define ESC_PULSE_MIN_US          1000   // zero throttle
#define ESC_PULSE_MAX_US          2000   // full throttle
#define ESC_MAX_THROTTLE_PCT      70.0f  // hard cap, enforced inside esc.c no matter what
#define ESC_ARM_MS                3000   // time held at zero throttle so the ESC arms

#define FLYWHEEL_THROTTLE_PCT     50.0f  // throttle while firing - tune on the bench
#define FLYWHEEL_RAMP_PCT_PER_SEC 40.0f  // gentle ramp so the supply doesn't see a current spike
#define FLYWHEEL_SETTLE_MS        500    // wait after reaching target before the first push
#define FLYWHEEL_HOLD_MS          5000   // keep spinning this long after the last shot

/* ---------- Dart pusher (MG90S, scotch yoke) ---------- */
#define PUSHER_SERVO_RANGE_DEG    180.0f
#define PUSHER_PULSE_MIN_US       500.0f
#define PUSHER_PULSE_MAX_US       2500.0f
#define PUSHER_RETRACT_DEG        20.0f
#define PUSHER_EXTEND_DEG        160.0f
#define PUSHER_EXTEND_MS         400      // time allowed to reach full extension
#define PUSHER_RETRACT_MS        400      // time allowed to get back and let the next dart drop