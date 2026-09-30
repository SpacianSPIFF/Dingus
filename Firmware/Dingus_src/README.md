# Dingus Turret Firmware

A 2-DOF (yaw + pitch) turret aimed by a handheld, IMU-based orientation controller.
The controller is held in the hand; tilting and rotating it points the turret the
same way. Built for the RMI test-rig assignment (Turret Mechanism topic).

Two ESP32 boards talk over ESP-NOW:

- **controller** - reads an MPU6050 IMU, fuses it into a roll/pitch/yaw estimate,
  reads two buttons, drives a small OLED status display, and transmits a command
  packet ~50 times a second.
- **turret** - receives that packet, applies safety limits, and drives two servos
  (yaw, pitch) plus (eventually) a flywheel ESC and a dart-pusher servo.

Both are ESP-IDF projects, sharing a small common component for the wire protocol
and ESP-NOW bring-up.

---

## Repository layout

```
Dingus_src/
├── components/
│   └── turret_common/        # code shared by BOTH boards
│       ├── include/
│       │   ├── packet.h            # wire format for controller -> turret packets
│       │   └── espnow_common.h
│       └── espnow_common.c         # WiFi/ESP-NOW bring-up, identical either side
│
├── controller/                # ESP-IDF project #1 - the handheld unit
│   └── main/
│       ├── main.c                  # orchestration: ties every module below together
│       ├── i2c_bus.c/.h             # one shared I2C bus (MPU6050 + OLED both sit on it)
│       ├── imu_mpu6050.c/.h         # raw accel/gyro reads from the MPU6050
│       ├── gyro_cal.c/.h            # startup gyro bias calibration
│       ├── orientation.c/.h         # complementary filter -> roll/pitch/yaw
│       ├── buttons.c/.h             # debounced, edge-triggered fire + mode buttons
│       ├── oled.c/.h                # SSD1306 128x32 status display driver
│       └── espnow_send.c/.h         # packs orientation into a packet, sends it
│
└── turret/                    # ESP-IDF project #2 - the turret unit
    └── main/
        ├── main.c                  # orchestration: failsafe, limits, servo output
        ├── espnow_recv.c/.h         # receives packets, tracks link freshness
        └── servo.c/.h               # generic servo PWM driver (ledc-based)
```

Each of `controller/` and `turret/` is built and flashed independently
(`cd controller && idf.py build flash monitor`, same for `turret/`).
`components/turret_common/` is pulled into both via `EXTRA_COMPONENT_DIRS`.

---

## Design principle: modular, not tangled

Every file except each project's `main.c` is written to be usable on its own,
with no knowledge of the rest of the system:

- `imu_mpu6050.c` knows how to talk to an MPU6050 over I2C. It does not know the
  result will be fused into an orientation, displayed, or transmitted.
- `oled.c` knows how to draw text to a 128x32 SSD1306. It does not know what the
  text *means*.
- `orientation.c` knows how to fuse gyro + accel into an angle estimate. It does
  not know where the gyro data came from or what happens to its output.
- `servo.c` knows how to turn an angle into a PWM signal on a given GPIO. It does
  not know it's driving a "yaw axis" specifically.
- `espnow_recv.c` knows how to receive and timestamp packets. It does not know
  what a "yaw" or "pitch" field means, or that a servo exists.

`main.c` in each project is the only place that understands what the turret rig
*is* - it reads from these modules, applies the actual control logic (limits,
calibration sequencing, failsafe behavior), and writes to them. This means any
single piece (say, the IMU, or the display) could be dropped into an unrelated
project unchanged.

---

## The wire protocol - `packet.h`

```c
typedef struct __attribute__((packed)) {
    uint32_t seq;        // incrementing counter
    float    yaw_deg;    // commanded yaw, workspace-limited on the turret side
    float    pitch_deg;  // commanded pitch, workspace-limited on the turret side
    uint8_t  trigger;    // edge-triggered: 1 only on the packet sent at the
                          // instant the fire button was pressed, 0 otherwise
} turret_cmd_t;
```

This is the only thing the two boards agree on. `__attribute__((packed))` forces
an identical byte layout on both sides regardless of compiler padding decisions -
without it, the two boards could silently disagree on the struct's memory layout
and every field after the first would read as garbage.

`seq` isn't used for ordering - it exists so the turret can log/debug which
packet it's acting on. The actual staleness check (see Failsafe, below) is based
on wall-clock time of arrival, not the sequence number.

---

## Controller side, module by module

### `i2c_bus.c/.h`
Creates a single I2C bus on the default pins (SDA=GPIO21, SCL=GPIO22). Both the
MPU6050 and the OLED are I2C devices at different addresses (`0x68` and `0x3C`)
and share this one bus - created once in `main.c`, handed to both device drivers.

### `imu_mpu6050.c/.h`
Wakes the MPU6050 from its power-on sleep state, configures ±2g / ±250°/s full
scale ranges, and exposes `mpu6050_read()` which returns raw accelerometer (g)
and gyroscope (deg/s) values. No filtering, no calibration - that's layered on
top by other modules.

### `gyro_cal.c/.h`
Runs once at boot, before the main loop starts. Samples the gyro for ~3 seconds
(the controller must be held still) and averages the result into a per-axis
bias. This bias is subtracted from every gyro reading from then on, which is
what prevents the yaw/roll/pitch estimate from drifting due to the gyro's
inherent zero-rate offset. Reports progress via a callback so the display module
never needs to know calibration exists.

### `orientation.c/.h`
A complementary filter: each axis is (mostly) the gyro reading integrated over
time, continuously pulled back toward whatever the accelerometer implies, at a
98%/2% blend. This corrects gyro drift on **roll and pitch**, because gravity
gives the accelerometer an absolute reference for those two axes. **Yaw has no
such reference** (rotating around the vertical axis doesn't change what gravity
looks like to the sensor) - without a magnetometer, yaw is pure integration and
will drift over time. This is a known, accepted limitation, mitigated by:

- A long-press (>1s) on the mode button calls `orientation_zero_yaw()`, letting
  the operator re-center yaw whenever drift becomes noticeable.

### `buttons.c/.h`
Two GPIOs (fire = GPIO4, mode = GPIO5), configured with internal pull-ups, read
as active-low. Exposes simple debounced, edge-triggered `button_*_pressed()`
functions - each returns `true` exactly once per physical press, not once per
loop iteration the button happens to be held down. The long-press-to-zero-yaw
gesture is currently detected separately, directly in `main.c`, since it needs
duration rather than a single edge event.

### `oled.c/.h`
A from-scratch SSD1306 128x32 driver: a 512-byte framebuffer, a hand-rolled 5x7
bitmap font, and I2C writes to push the buffer to the display. Exposes
`oled_draw_text(x, y, string)` - the caller decides what the text says; this
file only knows how to put pixels on the glass.

### `espnow_send.c/.h`
Registers the turret's MAC address as an ESP-NOW peer and exposes
`espnow_send_cmd()`, which fires a `turret_cmd_t` off over the air. Nothing more
- packing the struct with real orientation/trigger data happens in `main.c`.

### `main.c`
Boot sequence: bring up ESP-NOW → I2C bus → IMU → OLED → buttons → gyro
calibration (with a live percentage shown on-screen) → then the main ~50Hz loop:

1. Poll the mode button for a tap (toggle ORIENT/MANUAL) or a long-press (zero yaw).
2. Poll the fire button (edge-triggered).
3. Read the IMU, subtract calibration bias from the gyro values.
4. Run the complementary filter to update roll/pitch/yaw.
5. Pack yaw/pitch/trigger into a `turret_cmd_t` and send it.
6. Draw the current mode, roll/pitch/yaw, and a "FIRE" flash (held briefly so
   it's actually visible, since the underlying button press is only a single
   loop iteration long) to the OLED.

---

## Turret side, module by module

### `espnow_recv.c/.h`
Registers a receive callback for incoming ESP-NOW packets. Every time one
arrives, it's copied into a local "latest command" and timestamped. Exposes:

- `espnow_recv_get_latest()` - the most recent command received, if any.
- `espnow_recv_link_alive()` - `true` only if a packet has arrived within the
  last 500ms. This is the **failsafe** the brief requires: if the link drops,
  this flips to `false`, and `main.c` uses that to freeze the turret in place.

This module doesn't currently filter by sender MAC address - it will accept a
well-formed packet from anything broadcasting to it. Fine for a closed two-device
system; worth tightening if that assumption ever changes.

### `servo.c/.h`
A generic hobby-servo PWM driver built on the ESP32's `ledc` peripheral. One
shared 50Hz timer drives multiple independent channels (one per physical servo).
`servo_set_angle()` maps a 0–180° angle to a 500–2500µs pulse width - the
standard range confirmed against both the RDS5160 and MG946R datasheets - and
writes the corresponding PWM duty cycle. Clamps to 0–180° as a hard safety net;
the *real*, narrower workspace limits (see below) are applied one layer up.

### `main.c`
Boot sequence: bring up ESP-NOW receive → configure the shared servo timer →
init the yaw servo (GPIO18) and pitch servo (GPIO19). Then the main ~50Hz loop:

1. Pull the latest received command and check whether the link is still alive.
2. **If the link is dead, or nothing has ever arrived: do nothing.** The servos
   are simply never told to move, so they hold their last commanded position -
   satisfying the "hold last position and refuse to launch until the link is
   restored" requirement directly, rather than as a bolted-on special case.
3. Otherwise:
   - Clamp the commanded yaw to **±90°** and pitch to **0–60°** (the defined
     workspace from the brief).
   - Convert to servo-relative degrees (yaw is remapped from the controller's
     −90..+90 convention to the servo's 0..180 convention around its mechanical
     center).
   - Apply a **deadband** (ignore changes smaller than 0.5°, so sensor noise
     doesn't make the servo hunt) and a **slew-rate limit** (cap how many
     degrees the commanded angle may change per loop iteration, so the turret
     moves smoothly rather than snapping).
   - Write the resulting angles to both servos.
   - If `trigger` is set, fire - currently a placeholder log line, pending the
     actual firing-mechanism hardware and its GPIO.

---

## Known limitations / things still to do

- **Yaw drift** is expected and understood, not a bug - see the `orientation.c`
  section above. Mitigated by the long-press re-center gesture.
- **No sender authentication** on the turret's ESP-NOW receive - see
  `espnow_recv.c/.h` above.
- **Accelerometer validity isn't checked before blending** - a hard shake or
  free-fall would momentarily corrupt the roll/pitch estimate, since the filter
  currently assumes the accelerometer always reads ~1g of gravity. Not an issue
  for normal handheld aiming; worth revisiting if roll/pitch ever glitch during
  fast motion.
- **Firing mechanism** is not yet wired - `turret/main.c` logs the intent to
  fire but does not yet trigger any hardware.
- **Manual mode** (the second button's alternate mode) is tracked as a state but
  has no distinct behavior implemented yet.
- **Homing on boot**, **ballistic aim-assist**, and **telemetry streaming** are
  stretch objectives from the brief, not yet started.

---

## Building

```bash
cd controller && idf.py build flash monitor    # handheld unit
cd turret     && idf.py build flash monitor   # turret unit
```

Each project is independent - `idf.py` must be run from inside `controller/` or
`turret/`, never from the repository root.