#include <WiFi.h>

#define SENSOR_PIN 34  // SS49E connected to GPIO 34
#define VCC 3.3        // Supply voltage to the SS49E
#define SENSITIVITY 0.001  // Sensor sensitivity in V/G

unsigned long startTime = 0;
unsigned long lastReadTime = 0;
float interval = 2.0;  // Default interval in seconds
bool printTime = false;  // Flag to control whether to print time or 0
float baselineVoltage;

void setup() {
Serial.begin(115200);
analogReadResolution(12);  // Set ADC resolution to 12 bits

// Calibration
Serial.println("Calibrating sensor...");
delay(2000);
int rawValue = analogRead(SENSOR_PIN);
baselineVoltage = rawValue * (VCC / 4095.0);
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

if (newInterval >= 0.01) { // Prevent invalid values
interval = newInterval;
}

if (resetFlag == 1) {
startTime = millis();
lastReadTime = startTime;
printTime = true;
} else if (resetFlag == 0) {
startTime = millis();
lastReadTime = startTime;
printTime = false;
}
}
}

// Read and send magnetic field data at the specified interval
unsigned long currentTime = millis() - startTime;
if (currentTime - lastReadTime >= (interval * 1000)) {
lastReadTime = currentTime;

int rawValue = analogRead(SENSOR_PIN);
float voltage = rawValue * (VCC / 4095.0);
float magneticFieldGauss = (voltage - baselineVoltage) / SENSITIVITY;
float magneticFieldMilliTesla = (magneticFieldGauss / 10.0);  // Convert to mT

if (printTime) {
Serial.printf("%lu,%.3f,D\n", currentTime, magneticFieldMilliTesla);
} else {
Serial.printf("0,%.3f,D\n", magneticFieldMilliTesla);
}
}
}
