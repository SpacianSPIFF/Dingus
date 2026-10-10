# Dingus

**The Dignified Turret** — A 2-DOF (yaw + pitch) projectile-launching turret that mimics the orientation of a handheld controller in real time. Built for the **Robotics & Machine Intelligence (RMI) Test-Rig Assignment** (Topic 2.1: *Turret Mechanism*).

Point and tilt the handheld remote in your hand, and the turret follows wirelessly with low latency.

---

## System Overview

```
 ┌─────────────────────────────────────────────────────────┐
 │                   HANDHELD CONTROLLER                   │
 │                                                         │
 │  MPU6050 IMU ──(I2C)──► ESP32 ──(I2C)──► SSD1306 OLED   │
 │                          │                              │
 │  2x Pushbuttons ─────────┘                              │
 └──────────────────────────┬──────────────────────────────┘
                            │
                   ESP-NOW Wireless
                   (50 Hz, <10ms latency)
                            │
 ┌──────────────────────────▼──────────────────────────────┐
 │                      TURRET UNIT                        │
 │                                                         │
 │  ESP32 ──(LEDC PWM)──┬──► MG946R Servo (Yaw, 180°)      │
 │                      ├──► RDS5160 Servo (Pitch, 270°)   │
 │                      ├──► MG90S Servo (Dart Pusher)     │
 │                      └──► 2x SimonK ESCs + RS2212 BLDC  │
 └─────────────────────────────────────────────────────────┘
```

- **Controller:** Handheld remote built around an ESP32, an MPU6050 6-axis IMU, a 128×32 SSD1306 OLED, a dedicated fire button, and a debounced mode/calibration button. Raw accelerometer and gyroscope data are fused into orientation angles via an in-house complementary filter and streamed over ESP-NOW at 50 Hz.
- **Turret:** Receiver ESP32 driving a 2-DOF gimbal (MG946R for yaw, RDS5160 for pitch) with workspace limiting, software deadband, and slew-rate smoothing. Launches darts via dual counter-rotating brushless flywheels and an MG90S scotch-yoke pusher mechanism.
- **Failsafe:** If the wireless link drops for $> 500\text{ ms}$, the turret freezes its last commanded position and immediately cuts flywheel throttle to prevent accidental launches.

---

## Hardware & Pinout

### Controller Unit

| Component | Interface / Pin | Role | Notes |
|---|---|---|---|
| **ESP32** | — | Compute & ESP-NOW TX | Handheld enclosure |
| **MPU6050** | I2C (SDA: GPIO 21, SCL: GPIO 22) | 6-DOF IMU | 400 kHz, Address `0x68` |
| **SSD1306 OLED** | I2C (SDA: GPIO 21, SCL: GPIO 22) | HUD Status Display | 128×32, Address `0x3C` |
| **Fire Button** | GPIO 4 | Launch trigger | Active-low, 30 ms debounced edge |
| **Mode / Zero Button** | GPIO 5 | Mode toggle / Yaw reset | Tap: toggle mode; Hold ($\ge 800\text{ ms}$): zero yaw |

### Turret Unit

| Component | Interface / Pin | Role | Mechanical / PWM Specs |
|---|---|---|---|
| **ESP32** | — | Compute & ESP-NOW RX | Base rig electronics |
| **MG946R Servo** | GPIO 18 (LEDC Ch 0) | Yaw Axis | 180° range, 500–2500 µs pulse (Center: 90°) |
| **RDS5160 Servo** | GPIO 19 (LEDC Ch 1) | Pitch Axis | 270° range, 500–2500 µs pulse (0–60° workspace) |
| **MG90S Servo** | GPIO 23 (LEDC Ch 2) | Dart Pusher | 180° range, Scotch yoke (20° retract, 160° extend) |
| **ReadyToSky RS2212 920KV (x2)** | GPIO 25, 26 (LEDC Ch 3, 4) | Dual Flywheels | Driven by SimonK 30A ESCs (1000–2000 µs PWM) |

*See `Docs/img/Controller_Side_Circuit.png` and `Docs/img/Turret_Side_Circuit.png` for circuit diagrams.*

---

## Key Features & Architecture

### 1. In-House Sensor Fusion (No Black-Box Libraries)
Per the RMI test-rig requirements, orientation estimation is written from scratch in [`orientation.c`](./Firmware/Dingus_src/controller/main/orientation.c):
- High-responsiveness complementary filter ($\alpha = 0.92$, $\tau \approx 0.23\text{ s}$) blending high-rate gyro integration with accelerometer gravity tilt.
- Stationary gyro deadband ($0.4^\circ/\text{s}$) prevents drift when resting or held stationary.
- 3-second startup bias averaging in [`gyro_cal.c`](./Firmware/Dingus_src/controller/main/gyro_cal.c) with live progress percentage rendered to the OLED.

### 2. Tri-Mode Axis Control
The controller provides three distinct control modes, cycled sequentially on each single tap of the Mode button:
- **`MODE: ORIENT (Y+P)` (Default):** Full 2-DOF operation. Both Yaw and Pitch track your hand motions simultaneously.
- **`MODE: YAW ONLY`:** Locks Pitch at its current elevation angle and only allows Yaw (horizontal heading) to change. Perfect for sweeping a flat target line without vertical jitter.
- **`MODE: PITCH ONLY`:** Locks Yaw at its current heading angle and only allows Pitch (elevation) to change. Ideal for elevation tuning at a fixed azimuth.

*Holding the Mode button for $\ge 800\text{ ms}$ in any mode zeroes Yaw to $0^\circ$.*

### 3. Firing Sequence State Machine
Managed asynchronously by [`launcher.c`](./Firmware/Dingus_src/turret/main/launcher.c):
1. **Ramping:** On trigger, dual flywheels ramp smoothly at $40\%/\text{s}$ up to $50\%$ throttle to prevent battery voltage sag.
2. **Settling:** Waits $500\text{ ms}$ at target RPM for flywheel speed stabilization.
3. **Pusher Cycle:** MG90S extends to $160^\circ$ ($400\text{ ms}$) pushing the dart into the flywheels, then retracts to $20^\circ$ ($400\text{ ms}$).
4. **Idle Cooldown:** Flywheels remain spinning for $5\text{ s}$ after a shot, then spin down if no further shots occur.

### 4. Interactive OLED HUD with Graphical Status Icons & Launcher Telemetry
The 128×32 OLED is organized into a clean, real-time dashboard:
- **Row 0:** Active Mode (`MODE: ORIENT (Y+P)`, `MODE: YAW ONLY`, `MODE: PITCH ONLY`).
- **Row 1:** Commanded hand orientation angles (`CMD: P<deg>  Y<deg>`).
- **Row 2:** Live achieved turret angles (`TUR: P<deg>  Y<deg>`) echoed back continuously from the turret.
- **Row 3:** Live launcher state & alerts on the left, paired with a custom graphical Wi-Fi status symbol on the bottom right:
  - **Spooling:** `SPOOLING ...` (animated dots while brushless flywheels ramp to RPM).
  - **Firing:** `*** FIRING! ***` (dart pusher actively engaging the flywheels).
  - **Armed / Hot:** `HOT: READY (5s)` (flywheels at speed; counts down $5\text{ s} \to 1\text{ s}$ for instant rapid-fire).
  - **Zero / Link:** `YAW ZEROED!` or `LINK LOST`.
  - **Wi-Fi Icon (Bottom Right):** Radiating waves (`OK`), Slashed with `[X]` (`LOST`), or Checkmark `[✓]` (`REGAINED`).

---

## Repository Structure

```text
Dingus/
├── CAD/
│   ├── Controller/                # Remote enclosure (SolidWorks .SLDPRT)
│   └── Turret/                    # Barrel, gears, flywheel mounts (Fusion 360 .f3d)
├── Docs/
│   └── img/                       # Circuit schematics & wiring diagrams
├── Firmware/
│   └── Dingus_src/
│       ├── components/
│       │   └── turret_common/     # Shared packet wire protocol & Wi-Fi bringup
│       ├── controller/            # ESP-IDF Target 1: Handheld remote
│       │   └── main/              # IMU, OLED, Buttons, Orientation, ESP-NOW TX
│       └── turret/                # ESP-IDF Target 2: Physical turret
│           └── main/              # Servos, ESCs, Pusher, Launcher FSM, ESP-NOW RX
├── Buildlog.md                    # Progressive development log
├── README.md                      # Project documentation
└── Testrig.pdf                    # Assignment brief & performance targets
```

---

## Setup & Building

Both firmware targets are standard **ESP-IDF** (v5.x / v6.x) projects and are compiled independently.

### 1. Build and Flash the Controller
```bash
cd Firmware/Dingus_src/controller
idf.py set-target esp32
idf.py build flash monitor
```
*Hold the controller stationary on a flat surface for 3 seconds during power-up while the OLED displays `CALIBRATING`.*

### 2. Build and Flash the Turret
```bash
cd Firmware/Dingus_src/turret
idf.py set-target esp32
idf.py build flash monitor
```
*ESC takes ~3 seconds to arm.*

---

## Performance Targets (from `Testrig.pdf`)

- **Static Accuracy:** Commanded vs. achieved angle within $\pm 3^\circ$ across workspace.
- **Latency:** Visible response to controller motion under $100\text{ ms}$ end-to-end.
- **Repeatability:** Return to the same commanded pose 5 times and measure angular spread.
- **Stationary Drift:** Hold controller stationary for 60 seconds and verify turret does not wander.
