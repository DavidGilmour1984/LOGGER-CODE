#include <Arduino.h>

// =====================================================
// GATE PINS
// =====================================================
#define START_PIN 18
#define STOP_PIN  19

// =====================================================
// STATE (ISR-safe)
// =====================================================
volatile bool armed    = true;   // ready for START
volatile bool running  = false;
volatile bool finished = false;

volatile uint32_t tStart = 0;
volatile uint32_t tStop  = 0;

// =====================================================
// ISR: START (18)
// =====================================================
void IRAM_ATTR startISR() {
  if (armed && !running) {
    tStart = micros();
    running = true;
    armed = false;
  }
}

// =====================================================
// ISR: STOP (19)
// =====================================================
void IRAM_ATTR stopISR() {
  if (running && !finished) {
    tStop = micros();
    finished = true;
    running = false;
  }
}

// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(START_PIN, INPUT);
  pinMode(STOP_PIN,  INPUT);

  attachInterrupt(digitalPinToInterrupt(START_PIN), startISR, RISING);
  attachInterrupt(digitalPinToInterrupt(STOP_PIN),  stopISR,  RISING);

  Serial.println("READY");
}

// =====================================================
void loop() {

  // When a full trial completes, report ONCE
  if (finished) {
    uint32_t dt = tStop - tStart;

    Serial.print("TIME,");
    Serial.println(dt);   // microseconds

    // Reset state for next trial
    noInterrupts();
    armed    = true;
    running  = false;
    finished = false;
    tStart   = 0;
    tStop    = 0;
    interrupts();

    delay(50); // debounce safety
  }
}
