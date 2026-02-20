#include <Wire.h>
#include "SparkFun_SCD4x_Arduino_Library.h"

#define SDA_PIN 21  // ESP32 default I2C SDA
#define SCL_PIN 22  // ESP32 default I2C SCL

SCD4x scd41;

unsigned long startTime = 0;
unsigned long lastReadTime = 0;
float interval = 0.01;           // ✅ Default interval: 0.01s
bool printTime = true;           // ✅ Start printing actual time immediately

void setup() {
Serial.begin(115200);
Wire.begin(SDA_PIN, SCL_PIN);

if (!scd41.begin()) {
Serial.println("SCD41 not found!");
while (1);
}

Serial.println("ESP32 Serial Listening Started!");
startTime = millis();
lastReadTime = startTime;
}

void loop() {
// Check for serial input
if (Serial.available()) {
String input = Serial.readStringUntil('\n');
input.trim();

int commaIndex = input.indexOf(',');
if (commaIndex > 0) {
int resetFlag = input.substring(0, commaIndex).toInt();
float newInterval = input.substring(commaIndex + 1).toFloat();

startTime = millis();
lastReadTime = startTime;
printTime = (resetFlag == 1);

if (newInterval >= 0.01) {
interval = newInterval;
}
}
}

unsigned long currentTime = millis() - startTime;

if (currentTime - lastReadTime >= (interval * 1000)) {
lastReadTime = currentTime;

if (scd41.readMeasurement()) {
float co2 = scd41.getCO2();

if (printTime) {
Serial.printf("%lu,%.1f,F\n", currentTime, co2);  // ✅ CO2 only
} else {
Serial.printf("0,%.1f,F\n", co2);                 // ✅ CO2 only
}
}
}
}
Serial.printf("0,%.1f,%.1f,F\n", co2, humidity);  // Print 0 instead of time
}
}
}
}
