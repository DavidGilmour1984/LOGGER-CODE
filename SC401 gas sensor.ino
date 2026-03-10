#include <HardwareSerial.h>

HardwareSerial GAS(2);

#define RX_PIN 16
#define TX_PIN 17

void setup()
{
Serial.begin(9600);

GAS.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

Serial.println("SC401 UART Sniffer Started");
}

void loop()
{
static uint8_t frame[12];
static int index = 0;

while(GAS.available())
{
uint8_t b = GAS.read();

if(index == 0 && b != 0x3C) return;

frame[index++] = b;

if(index == 12)
{
uint16_t coRaw  = (frame[2] << 8) | frame[3];
uint16_t h2sRaw = (frame[4] << 8) | frame[5];
uint16_t so2Raw = (frame[6] << 8) | frame[7];
uint16_t o2Raw  = (frame[8] << 8) | frame[9];

float co  = coRaw  / 10.0;
float h2s = h2sRaw / 10.0;
float so2 = so2Raw / 10.0;
float o2  = o2Raw * 0.65;   // calibration factor for %

Serial.print("CO: ");
Serial.print(co);
Serial.print(" ppm   ");

Serial.print("SO2: ");
Serial.print(so2);
Serial.print(" ppm   ");

Serial.print("H2S: ");
Serial.print(h2s);
Serial.print(" ppm   ");

Serial.print("O2: ");
Serial.print(o2);
Serial.println(" %");

index = 0;
}
}
}
