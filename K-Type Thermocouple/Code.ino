#include "max6675.h"

// Define the pins for the MAX6675 sensor
int thermoDO = 19;   // SO (D19)
int thermoCS = 5;    // CS (D5)
int thermoCLK = 18;  // SCK (D18)

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// Variables for timing
unsigned long startTime = 0;
unsigned long lastReadTime = 0;
float interval = 2.0; // Default interval in seconds
bool printTime = false;  // Flag to control whether to print time or 0

void setup() {
Serial.begin(115200);
Serial.println("MAX6675 test started!");

// Wait for MAX6675 chip to stabilize
delay(500);

startTime = millis();
}

void loop() {
// Check for serial input
if (Serial.available()) {
String input = Serial.readStringUntil('\n');
input.trim(); // Remove any whitespace

int commaIndex = input.indexOf(',');
if (commaIndex > 0) {
int resetFlag = input.substring(0, commaIndex).toInt();
float newInterval = input.substring(commaIndex + 1).toFloat();

// Handle reset flag and interval
if (resetFlag == 1) {
startTime = millis(); // Reset start time
lastReadTime = startTime; // Reset last read time
printTime = true;  // Start printing time
} else if (resetFlag == 0) {
startTime = millis(); // Reset start time
lastReadTime = startTime; // Reset last read time
printTime = false;  // Print 0 instead of time
}

if (newInterval >= 0.01) { // Prevent 0 or negative values
interval = newInterval;
}
}
}

unsigned long currentTime = millis() - startTime;

if (currentTime - lastReadTime >= (interval * 1000)) {
lastReadTime = currentTime;

// Read temperature from MAX6675
float max6675Temp = thermocouple.readCelsius();  // Get temperature in Celsius

if (printTime) {
Serial.printf("%lu,%.1f,C\n", currentTime, max6675Temp);  // Print time and temperature
} else {
Serial.printf("0,%.1f,C\n", max6675Temp);  // Print 0 instead of time
}
}
}
