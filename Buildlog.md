# Build Log

## 2026-09-30

**Focus:** Maximise progress

**Summary:**
- The Mode change and Yaw calibrate button doesn't work, not sure why. Yaw calibrate basically being that the yaw shoots off randomly overtime, which it shouldn't, and so to fix that if one holds the button, it resets yaw to 0
- Fixed code bug where the Roll and Pitch is swapped

## 2026-09-29

**Focus:** Finishing of the code and model

**Summary:**
- Finally got the controller packaged, and flashed. Looks like the ["Finishing the controller"](#2026-09-23) never actually finished... But fine, got it done now
- Well, I finished _almost_ the entire code today... (yes, I used AI, but it's fine for now)
- Put parts to print, but turns out many flaws in modelling, like the dimensions, and the further mount planning, so gotta redo
- Started soldering the perfboard for the turret

## 2026-09-28

**Focus:** Writing code for the controller

**Summary:**
- Finished wiring the controller, though many times a loose conneciton makes the ESP reset by itself, for I believe something is pulling a bus low. Now for that I need to now figure out how to fix a perfboard in the _severly_ limited volume of the controller, as i designed it without a perfboard in mind... Maybe I can reprint it without the standoffs?
- Wrote significant portion of the code for the controller
- Tested th ecode currently wrote... Builds and displays on the OLED also
- Next up is the ESP-NOW and the turret code left
- I have been putting off the modelling for way too long for the turret. hae to finish that ASAP

## 2026-09-23

**Focus:** Finishing the controller

**Summary:**
- Finished the CAD model and printing of the controller
- The base had many issues, fixed them one by one, primarily being the dimensions
- Started wiring the electronics on the controller

## 2026-09-19

**Focus:** Move modelling to Solidworks, and finish off the controller. Also initialising the code base

**Timeline:** Majorly the previous week

**Summary:**
- Well, I restarted modelling everything from scratch in Solidworks, as RMI prmiarily works on it, and not Fusion.
- So I primarily worked on modelling the controller. Thinking about how I should design it. Initially my idea was to make it like the [Nintendo Wii's Nunchuk](https://nintendo.fandom.com/wiki/Nunchuk), But figuring out the component placement + complexity made me reconsider, and I moved to a much simpler TV remote style. So initially my idea was to go completely screwless for the controller, making it look aesthetic, but then when I tried *press fit* with different tolerances, it just wouldn't work. Later while assembly I realised the sheer blunder I commited, while not taking into account on how to even place the components, and so, came the idea of standoffs, where each component would sit on these standoffs, and have enough space on the bottom for wires to go through, and so each components' height can be individually adjusted.
- I also initialised the code base for ESP-IDF, and under 1 project of ESP-IDF (that's how it considers?), I made like 2 projects in them? One for the controller, and one for the turret. They share the same communication protocol to send data via ESP-NOW, so a common `protocol.h` exists, just to ensure that happens. I'm yet to start full fledged programming, but yeah, the basic setup has been done for it, and I made sure to update the `CMakeLists.txt` files.
- And yes, finally understood that buil-logs are just putting down how we failed and progressed, so finally doing that :P
- Though one concern does exist, where my assemblies are touching 10-12MB each, and git storing binaries would just inflate the `.git` folder...? I don't know. Let's see.
- Oh also, just changed the folder structure a bit (I might keep doing that till I satisfy myself)
- I think I should maybe update the `README.md` file also soon.
- P. S. Been a while, but it's good to be back :)

## 2026-08-23

**Focus:** Continued modelling, and started electronic design

**Summary:**
- Made the circuit block diagrams
- Made CAD model for Reload Gear

## 2026-08-22

**Focus:** Finalised on the firing mechanism, and started modelling

**Summary:**
- Made CAD models for:
  - Dart
  - Firing Nozzle
  - Flywheel Base
  - Flywheel

## 2026-08-21

**Focus:** Ideation for firing mechanism for the turret

**Summary:**
- Was able to come up with 3 methods of firing:
  1. High speed fly wheel with a dart pusher + dart loader mechanism with grooved gears. A variant of it can also be done for beads, where the gear has dimples on it instead of grooves.
  2. Use a spring with a plate in front of it to launch the bead. The plate has a hook, with thread tied to it, which connects on one side of ratchet and pawl mechanism, and the other side of it is connected to a motor, which enables free flow of the spring in one direction, and helps pull the spring back in the other direction.
  3. In the first mechanism itself, if a bead is kept, once it is loaded, instead of flywheel, use a paddle to hit the bead at high speed, like a pinball paddle.

## 2026-08-14

**Focus:** Thought of the entire design of the turret

**Summary:**
- Initialised GitHub repo
- Set up basic project structure
