// Step 1 of the MIDI firmware: prove the toolchain.
// Blinks the board's own RGB LED (GPIO 48) dimly and prints a counter
// to the serial port (the UART socket, 115200 baud).
#include <Arduino.h>

const uint8_t ONBOARD_LED_PIN = 48;

// Brightness ceiling, 0-255. Nothing in this firmware may exceed it.
// The cape is for a dark stage: less is more.
const uint8_t MAX_BRIGHTNESS = 16;

void setup() {
  Serial.begin(115200);
}

void loop() {
  static uint32_t count = 0;

  rgbLedWrite(ONBOARD_LED_PIN, 0, 0, MAX_BRIGHTNESS);  // dim blue
  delay(500);
  rgbLedWrite(ONBOARD_LED_PIN, 0, 0, 0);
  delay(500);

  Serial.printf("blink %lu\n", (unsigned long)++count);
}
