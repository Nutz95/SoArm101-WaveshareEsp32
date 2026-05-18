# Dual Arm Teleoperation Plan

## Scope

This plan covers a two-board ESP32 firmware system for Waveshare STS32xx serial bus servos:
- Leader board: Servo Driver with ESP32 + OLED + 2x WS2812B + Bluetooth gamepad pairing.
- Follower board: Bus Servo Driver HAT (A) + ESP-NOW receiver and servo replication.

The initial delivery target is USB build and upload from PlatformIO (no OTA, no WiFi setup).

## Hardware Targets

- Leader board USB port: COM7
- Follower board USB port: COM8
- Leader RGB LEDs: 2x WS2812B on IO23 (serial chain)
- Leader OLED: I2C on IO22 (SCL) and IO21 (SDA)
- Servo bus: UART path exposed through board servo bus interface

## Engineering Constraints

- Language for committed files and code: English only.
- Design goals: SOLID, DRY, KISS.
- One class per file.
- Clear, descriptive names for symbols.
- Persistent settings and calibration data in NVS are mandatory.
- Architecture must support adding future modes with minimal changes.

## Planned Architecture

- Shared module layer (`src_dual_arm/common`):
  - Mode/state model
  - State machine
  - NVS persistence service
  - LED status service
  - ESP-NOW link abstraction
  - Servo abstraction
- Leader app layer (`src_dual_arm/leader`):
  - Bluetooth gamepad adapter (NimBLE)
  - OLED status presenter
  - Leader mode controller
- Follower app layer (`src_dual_arm/follower`):
  - Follower mode controller
  - Servo mirror executor

## State Model (Initial)

Each arm exposes an independent state used by LED/OLED rendering.

- Blue blinking: pairing in progress or not paired (leader only)
- Blue solid: pairing completed (leader only)
- Red blinking: waiting calibration
- Green blinking: waiting ESP-NOW link
- Green solid: ready

Operation modes (extensible):
- Idle
- CalibrationLeader
- CalibrationFollower
- Teleoperation

## Communication Model

- Leader owns gamepad input and high-level mode decisions.
- Leader sends mode and servo position packets to follower through ESP-NOW.
- Follower applies packeted target positions to local servos.
- Sequence counters and freshness timeout will be used for safety.

## Calibration Model

- Calibrate min/max for each servo on both arms.
- Persist calibration in NVS with versioned keys.
- Load calibration at boot and validate ranges.
- Provide reset-to-default path.

## Implementation Checklist

### 0. Bootstrap
- [x] Create project plan document and tracking checklist.
- [x] Create dual-target PlatformIO environments (leader/follower).
- [x] Add Windows PowerShell scripts for build and upload.

### 1. Core Domain
- [x] Add role/mode/state enums and shared state DTOs.
- [x] Add finite state machine with explicit transitions.
- [x] Add NVS persistence service (keys, schema version, defaults).
- [ ] Add structured protocol definitions for ESP-NOW packets.

### 2. Board Services
- [x] Add RGB status LED driver service.
- [ ] Add leader OLED status presenter service.
- [ ] Add servo bus abstraction and concrete STS32xx service.
- [ ] Add ESP-NOW transport service with callbacks.

### 3. Leader Features
- [ ] Integrate NimBLE gamepad client and pairing flow.
- [ ] Map controller buttons to mode commands.
- [ ] Add leader arm position sampling and packet publish.
- [ ] Add local/remote state merge for dual LED rendering.

### 4. Follower Features
- [ ] Add packet receiver and command dispatcher.
- [ ] Apply target servo positions with limit checks.
- [ ] Publish follower heartbeat/state to leader.

### 5. Calibration Workflow
- [ ] Add calibration state entry/exit handlers.
- [ ] Add min/max capture and validation.
- [ ] Save/load calibration profiles from NVS.

### 6. Safety and Robustness
- [ ] Add comms timeout fallback state handling.
- [ ] Add invalid-packet filtering and counters.
- [ ] Add startup self-check and failure reporting.

### 7. Tests and Validation
- [ ] Add host-side unit tests for state machine and packet encode/decode.
- [ ] Add unit tests for NVS key mapping logic (mocked).
- [ ] Validate build on both envs from scripts.
- [ ] Bench test with COM7 leader and COM8 follower.

### 8. Documentation
- [ ] Update firmware README with build/upload commands.
- [ ] Document pin mapping and expected LED state semantics.
- [ ] Document controller button map.

## Immediate Next Steps

1. Add ESP-NOW transport skeleton and packet protocol.
2. Replace startup placeholders with real joystick and link events.
3. Add servo bus abstraction and STS32xx implementation.
4. Add OLED status presenter on leader.
5. Keep Bluetooth and OLED integration behind interfaces to avoid coupling.
