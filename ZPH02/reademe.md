# ZPH02 UART Frame Reader (ESP32)

This sketch reads and decodes the **UART data stream from a ZPH02 air-quality sensor** using an ESP32. It prints the **raw frame bytes and interpreted values** to the Serial Monitor.

The sensor transmits a **9-byte frame once per second**, which contains PM2.5 pulse information and a VOC air-quality grade.

---

# Hardware

## Required components

- ESP32
- ZPH02 air-quality / particle sensor
- 5 V power supply for the sensor
- Jumper wires

---

# Wiring

ZPH02 pinout:

| Sensor Pin | Function | ESP32 |
|---|---|---|
| PIN1 | MOD | GND |
| PIN2 | RXD | GPIO27 |
| PIN3 | VCC | 5 V |
| PIN4 | TXD | GPIO26 |
| PIN5 | GND | GND |

Important notes:

MOD must be connected to **GND** to enable **UART mode**.  
If MOD is left floating the sensor switches to **PWM output mode**.

---

# Serial settings

UART parameters used by the sensor:

| Setting | Value |
|---|---|
| Baud rate | 9600 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Update rate | ~1 Hz |

The ESP32 reads the sensor using **UART2**.

---

# Data frame format

Each frame is **9 bytes**:

| Byte | Meaning |
|---|---|
| 0 | Start byte (0xFF) |
| 1 | Detection type (0x18) |
| 2 | Unit |
| 3 | Low pulse integer |
| 4 | Low pulse decimal |
| 5 | Reserved |
| 6 | Mode |
| 7 | VOC grade |
| 8 | Checksum |

Example frame:

FF 18 00 00 32 00 01 00 B5


---

# Interpreting values

## PM2.5 low pulse rate

Low pulse rate = Byte3 + Byte4 / 100


Example:

Byte3 = 0
Byte4 = 32

Low pulse rate = 0.32 %


Typical air quality ranges:

| Pulse rate | Air quality |
|---|---|
| 0–4 % | Best |
| 4–8 % | Good |
| 8–12 % | Bad |
| >12 % | Worst |

---

## VOC grade

| Value | Meaning |
|---|---|
| 0 | Sensor error or VOC not available |
| 1 | Best |
| 2 | Good |
| 3 | Bad |
| 4 | Worst |

---

# Serial output

Example output:

ZPH02 full frame reader

------ FRAME ------
Start byte: FF
Detection type: 18
Unit byte: 00
Low pulse integer: 0
Low pulse decimal: 32
PM2.5 low pulse rate (%): 0.32
Reserved: 0
Mode: 1
VOC grade: 0
Checksum: B5


---

# How the sketch works

1. The ESP32 listens to UART2 for incoming data.
2. It waits for the **start byte (0xFF)**.
3. The remaining **8 bytes of the frame are read**.
4. Each byte is printed and interpreted.
5. The PM2.5 pulse rate and VOC grade are calculated and displayed.

---

# Warm-up time

The sensor requires a **warm-up period of about 5 minutes** before readings stabilize.

---

# Notes

- The ZPH02 operates at **5 V** and can draw up to **150 mA**.
- UART communication is **one-way**; the sensor continuously sends data and does not require commands.
