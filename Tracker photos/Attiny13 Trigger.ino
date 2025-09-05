#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile uint8_t wdtCounter = 0;

ISR(WDT_vect) {
  wdtCounter++;
}

void setupWatchdog() {
  cli();
  // Enable configuration changes
  WDTCR = (1 << WDCE) | (1 << WDE);
  // Set to interrupt mode only, 8s interval
  WDTCR = (1 << WDTIE) | (1 << WDP3) | (1 << WDP0); 
  sei();
}

void goToSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

int main(void) {
  // Pin 5 (PB0) as output
  DDRB |= (1 << PB0);
  PORTB &= ~(1 << PB0); // start LOW

  setupWatchdog();

  while (1) {
    if (wdtCounter >= 8) {  // ~64 seconds
      wdtCounter = 0;

      PORTB |= (1 << PB0);  // HIGH
      _delay_ms(300);
      PORTB &= ~(1 << PB0); // LOW
    }

    goToSleep();
  }
}
