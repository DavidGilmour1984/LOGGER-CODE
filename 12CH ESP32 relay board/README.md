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
```

This ensures:
- Button “R1” controls physical Relay 1
- Button “R16” controls Relay 16

---

### 3. Safe Startup (Important)
On power-up:
1. Outputs are disabled (OE HIGH)
2. All relay states are cleared
3. Data is written to the shift registers
4. Outputs are enabled (OE LOW)

This prevents random relays turning on at boot.

---

### 4. WiFi Access Point
The ESP32 creates a WiFi network:

- SSID: ESP32 Relay
- Password: 12345678
- IP Address: 192.168.4.1

No internet is required.

---

### 5. Web Interface

Open in your browser:

http://192.168.4.1

You get:
- 16 large toggle buttons (2 columns × 8 rows)
- Each button shows ON (green) or OFF (red)
- Clicking toggles the relay instantly

---

### 6. Signal Strength
The page displays WiFi signal strength as a percentage:
- Based on RSSI
- Updated each page load

---

## Usage Instructions

1. Power the ESP32 and relay board  
2. Connect to WiFi: ESP32 Relay  
3. Open browser: 192.168.4.1  
4. Tap buttons to control relays  

---

## Notes on Security

Current setup:
- WPA2 WiFi password (basic protection)
- No login page

This is:
- ✔ Suitable for local/private use  
- ❌ Not secure for public or internet exposure  

---

## Troubleshooting

### Relays turn on randomly at startup
- Ensure OE is controlled as in code
- Add a 10k pull-up resistor from OE → VCC (recommended)

### Relays not responding
- Check wiring (DATA, CLOCK, LATCH)
- Confirm 12V supply is connected
- Ensure common ground

### Wrong relay activates
- Mapping is handled in code
- If still incorrect, board wiring may differ

---

## Future Improvements
- Live status updates (no page refresh)
- Button state persistence
- Authentication system
- MQTT / Home Assistant integration

---

## Summary
This system provides a simple, reliable way to control 16 relays wirelessly using an ESP32 and shift registers, with a clean web interface and stable startup behaviour.
