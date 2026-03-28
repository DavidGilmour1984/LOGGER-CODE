# ESP32 16-Relay WiFi Controller (74HC595)

## Overview
This project turns an ESP32 into a wireless access point that controls a 16-relay board driven by two cascaded 74HC595 shift registers.

You connect to the ESP32 via WiFi, open a web page, and toggle relays in real time using large on-screen buttons.

---

## Features
- 16 relay control via two 74HC595 chips
- ESP32 creates its own WiFi network (Access Point)
- Web-based control interface (no app required)
- Toggle buttons with live ON/OFF state
- Signal strength (%) display
- Safe startup (no random relay activation)

---

## Hardware Required
- ESP32
- 16-channel relay board using 74HC595 (or equivalent)
- 12V power supply for relays
- Common ground between ESP32 and relay board

---

## Wiring

| ESP32 Pin | Relay Board |
|----------|------------|
| GPIO12   | LATCH      |
| GPIO13   | CLOCK      |
| GPIO14   | DATA       |
| GPIO5    | OE         |
| GND      | GND        |

⚠️ The relay board must be powered separately (typically 12V), but must share **GND** with the ESP32.

---

## How It Works

### 1. Shift Register Control
The system uses two cascaded 74HC595 shift registers:

- Each chip = 8 outputs  
- Total = 16 relays  

The ESP32 sends a 16-bit value:
- Each bit represents a relay state (ON/OFF)
- Data is shifted out using:
  - DATA (serial data)
  - CLOCK (bit timing)
  - LATCH (apply outputs)

---

### 2. Relay Mapping
Because of the physical wiring of the board, relay order is reversed.

The code corrects this using:

```cpp
bit = 15 - (relay_number - 1);
