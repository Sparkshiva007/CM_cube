# CM³ – Centralized Multi-Device Mode Management Engine

CM³ is a distributed, mode-based home automation system designed to **adapt automatically to user context** instead of relying on manual control or rigid schedules.  
It uses a **central intelligence node** with **multiple distributed sensor and control nodes**, communicating over **MQTT**.

The system focuses on **practical automation**, low user effort, and real-world deployability.

---

## Core Idea

Traditional home automation systems control individual devices.

**CM³ controls modes.**

A *mode* represents a complete system state (for example: Sleep, Study, Game, Emergency).  
When environmental conditions change, CM³ evaluates rules and automatically switches modes, coordinating multiple devices together.

---

## System Architecture

### Main Unit (Core Engine)
- ESP32-S3 Box 3
- Runs CM³ rule engine
- Hosts LVGL-based UI
- Publishes global system mode
- Subscribes to sensor telemetry
- Handles manual override and safety logic

### Slave Units (Distributed Nodes)
- ESP32-C3 and ESP32-C6
- Handle local sensing and relay control
- Publish sensor data via MQTT
- Subscribe to mode and command topics
- Operate independently with fail-safe defaults

---

## Hardware Used

### Main Unit
- ESP32-S3 Box 3 (Display + Touch)
- Wi-Fi connectivity
- LVGL + LovyanGFX

### Slave Node Hardware
- ESP32-C3 / ESP32-C6 (Glyph boards)
- DHT22 temperature & humidity sensor
- PIR motion sensor
- LDR light sensor module
- 4-channel relay module
- External AC / DC loads

---

## Communication Protocol

- Protocol: MQTT
- Broker: Any standard MQTT broker (HiveMQ, Mosquitto, etc.)
- Architecture: Publish / Subscribe
- No direct device-to-device coupling

### MQTT Topic Structure

```text
cm3/node/c3/telemetry
cm3/node/c6/telemetry

cm3/node/c3/status
cm3/node/c6/status

cm3/system/mode
cm3/system/manual
cm3/system/override

cm3/device/c3/cmd
cm3/device/c6/cmd
