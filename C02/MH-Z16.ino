#include <Arduino.h>

/*
========================================================
MH-Z16 CO2 SENSOR — ESP32 (UART2)

READ ME FIRST
--------------------------------------------------------
• Sensor: MH-Z16 / MH-Z19 (UART mode)
• Power: MUST be 5V
• UART: 9600 baud, 8N1
• Pins:
    MH-Z TX  -> ESP32 GPIO26
    MH-Z RX  -> ESP32 GPIO27
    GND      -> GND
• Warm-up: 2–3 minutes for stable readings

WHAT THIS CODE DOES
--------------------------------------------------------
• Sends proper read command
• Waits for full 9-byte response
• Validates header + checksum
• Decodes CO₂ ppm and temperature
• Applies moving-average filter
• Rejects spikes and invalid frames
• Prints clean, stable output

OUTPUT FORMAT
--------------------------------------------------------
CO2: #### ppm | Temp: ## C

========================================================
*/

HardwareSerial CO2(2);

// MH-Z read command
const uint8_t READ_CMD[9] = {
  0xFF, 0x01, 0x86,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x79
};

// filtering
#define FILTER_SIZE 5
int ppmBuffer[FILTER_SIZE];
int bufIndex = 0;
bool bufferFilled = false;

// timing
unsigned long lastPoll = 0;
const unsigned long POLL_INTERVAL = 2000; // ms

// =====================================================
// checksum validation
// =====================================================
bool validChecksum(uint8_t *buf) {
  uint8_t sum = 0;
  for (int i = 1; i < 8; i++) sum += buf[i];
  sum = 0xFF - sum + 1;
  return sum == buf[8];
}

// =====================================================
// filtered average
// =====================================================
int filteredPPM(int ppm) {
  ppmBuffer[bufIndex++] = ppm;
  if (bufIndex >= FILTER_SIZE) {
    bufIndex = 0;
    bufferFilled = true;
  }

  int count = bufferFilled ? FILTER_SIZE : bufIndex;
  int total = 0;
  for (int i = 0; i < count; i++) total += ppmBuffer[i];
  return total / count;
}

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("MH-Z16 CO2 READER STARTED");
  Serial.println("--------------------------------");

  CO2.begin(9600, SERIAL_8N1, 26, 27);
}

// =====================================================
void loop() {

  if (millis() - lastPoll < POLL_INTERVAL) return;
  lastPoll = millis();

  // flush any junk
  while (CO2.available()) CO2.read();

  // send command
  CO2.write(READ_CMD, 9);

  // wait for response
  unsigned long t0 = millis();
  while (CO2.available() < 9) {
    if (millis() - t0 > 300) return; // timeout
  }

  uint8_t buf[9];
  CO2.readBytes(buf, 9);

  // validate frame
  if (buf[0] != 0xFF || buf[1] != 0x86) return;
  if (!validChecksum(buf)) return;

  int ppm  = (buf[2] << 8) | buf[3];
  int temp = buf[4] - 40;

  // reject garbage
  if (ppm < 300 || ppm > 5000) return;

  int smoothPPM = filteredPPM(ppm);

  Serial.print("CO2: ");
  Serial.print(smoothPPM);
  Serial.print(" ppm | Temp: ");
  Serial.print(temp);
  Serial.println(" C");
}
