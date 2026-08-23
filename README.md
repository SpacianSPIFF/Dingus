# Dingus

The dignified turret.

A 2-DOF (yaw + pitch) turret that mimics the orientation of a handheld controller in real time. Tilt and rotate the controller, and the turret follows. Powered by an ESP32 + IMU on the controller side, talking wirelessly to an ESP32 on the turret side.

## Overview

- **Controller:** ESP32 + MPU6050 IMU + fire button. Reads raw IMU data, runs sensor fusion to estimate orientation, and streams pose + fire commands to the turret over Wi-Fi.
- **Turret:** ESP32 receives commands and drives yaw (servo), pitch (servo), a flywheel (brushless motor) for launching, a dart-push motor, and a reload-gear motor.

See `Docs/img/Controller_Side_Circuit.png` and `Docs/img/Turret_Side_Circuit.png` for the wiring block diagrams.

## Status

Work in progress. See [`Buildlog.md`](./Buildlog.md) for progress.

## Repo Structure

```text
Dingus/
├── CAD/                    # 15 parts — turret assembly + controller enclosure
├── Docs/
│   └── img/                # circuit diagrams + wiring photos
├── Firmware/
├── Buildlog.md
├── README.md
└── Testrig.pdf
```

## Setup & Running

> TODO: fill in once firmware is in a runnable state.

## Hardware

| Component | Role |
|---|---|
| ESP32 (x2) | Controller + Turret compute |
| MPU6050 | IMU for orientation sensing |
| RDS5160 Servo | Yaw |
| MG995 Servo | Pitch |
| Brushless Motor | Flywheel (launch) |
| DC Motor | Dart push |
| DC Motor | Reload gear |
