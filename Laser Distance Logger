#include <Adafruit_VL53L0X.h>

// Create an object of the Adafruit_VL53L0X class
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// Variables for timing
unsigned long startTime = 0;
unsigned long lastReadTime = 0;
float interval = 0.01; // Default interval in seconds
bool printTime = false;  // Flag to control whether to print time or 0

void setup() {
Serial.begin(115200);
Serial.println("Adafruit VL53L0X test started!");

// wait until serial port opens for native USB devices
while (!Serial) {
delay(1);
}

// Initialize the sensor
if (!lox.begin()) {
Serial.println(F("Failed to boot VL53L0X"));
while (1);  // Stop the program if the sensor cannot be initialized
}

// Initialize timing
startTime = millis();
lastReadTime = startTime;
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

// Check if enough time has passed to read the sensor
if (currentTime - lastReadTime >= (interval * 1000)) {
lastReadTime = currentTime;

VL53L0X_RangingMeasurementData_t measure;
lox.rangingTest(&measure, false);  // pass in 'true' to get debug data printout!

if (measure.RangeStatus != 4) {  // phase failures have incorrect data
if (printTime) {
Serial.printf("%lu,%d,G\n", currentTime, measure.RangeMilliMeter);  // Print time and distance
} else {
Serial.printf("0,%d,G\n", measure.RangeMilliMeter);  // Print 0 instead of time
}
} else {
if (printTime) {
Serial.printf("%lu,0,G\n", currentTime);  // Print time and out of range value
} else {
Serial.printf("0,0,G\n");  // Print 0 instead of time and out of range value
}
}
}
}
