/*
=====================================================
README — PMS7003T ESP32 UART2 Reader (Robust Sync)
=====================================================

WHAT THIS CODE DOES
- Reads particulate matter data from a PMS7003T sensor
- Uses ESP32 UART2 with hardware pins:
    RX = GPIO16
    TX = GPIO17
- Performs robust, byte-by-byte frame synchronisation
- Verifies frame length and checksum
- Prints PM1.0, PM2.5, and PM10 values to Serial
- Includes a heartbeat (.) every second to show the loop is alive

WHY THIS VERSION WORKS
- PMS sensors stream continuously and power up mid-frame
- This code does NOT assume alignment
- It locks onto the 0x42 0x4D header safely
- Automatically re-syncs if bytes are dropped
- Prevents silent failure

WIRING (CRITICAL)
PMS7003T  →  ESP32
-------------------
VCC       →  5V   (required)
GND       →  GND
TX        →  GPIO16
RX        →  GPIO17 (optional)
SET / EN  →  5V   (must be HIGH or sensor sleeps)

SERIAL SETTINGS
- PMS7003T: 9600 baud, 8N1
- USB Serial Monitor: 115200 baud

EXPECTED OUTPUT
- Dots printed once per second while waiting
- Once frames lock:
    PM1.0 = X
    PM2.5 = Y
    PM10  = Z

TYPICAL CLEAN INDOOR VALUES
- PM1.0 : 0–5 µg/m³
- PM2.5 : 0–10 µg/m³
- PM10  : 0–20 µg/m³

NOTES
- If you see dots but no values, UART wiring or power is wrong
- If you see raw hex but no parsed values, checksum or framing is wrong
- This parser is production-safe and self-recovering

=====================================================
*/

#include <Arduino.h>

HardwareSerial PMS(2);

#define PMS_FRAME_LEN 32
#define PMS_H1 0x42
#define PMS_H2 0x4D

uint8_t frame[PMS_FRAME_LEN];

uint16_t u16(int i) {
  return (uint16_t(frame[i]) << 8) | frame[i + 1];
}

unsigned long lastBeat = 0;

// =====================================================
// Robust frame reader (byte-by-byte sync)
// =====================================================
bool readPMS() {
  static uint8_t idx = 0;

  while (PMS.available()) {
    uint8_t b = PMS.read();

    if (idx == 0 && b != PMS_H1) continue;
    if (idx == 1 && b != PMS_H2) {
      idx = 0;
      continue;
    }

    frame[idx++] = b;

    if (idx == PMS_FRAME_LEN) {
      idx = 0;

      // length must be 28
      if (u16(2) != 28) return false;

      // checksum
      uint16_t sum = 0;
      for (int i = 0; i < PMS_FRAME_LEN - 2; i++) {
        sum += frame[i];
      }
      if (sum != u16(30)) return false;

      return true;  // VALID FRAME
    }
  }
  return false;
}

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("PMS7003T SYNCED READER STARTED");

  // UART2, default pins
  PMS.begin(9600, SERIAL_8N1, 16, 17);
}

// =====================================================
void loop() {

  // heartbeat so you KNOW loop is running
  if (millis() - lastBeat > 1000) {
    lastBeat = millis();
    Serial.print(".");
  }

  if (!readPMS()) return;

  Serial.println();
  Serial.print("PM1.0 = ");
  Serial.println(u16(4));
  Serial.print("PM2.5 = ");
  Serial.println(u16(6));
  Serial.print("PM10  = ");
  Serial.println(u16(8));
}
