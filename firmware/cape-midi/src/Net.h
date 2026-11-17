// Wi-Fi, for development at home only. It gives three things, all on the
// cape's address in a browser or with curl:
//
//     http://<address>/         what is running and how it is doing
//     http://<address>/log      the last few thousand characters it has printed
//     POST   <address>/update   a new firmware image (tools\pio run -e wifi -t upload)
//
// Bluetooth and Wi-Fi share one radio. So if the home network in secrets.h is
// not found within a few seconds of power-up (a gig, a rehearsal room), Wi-Fi
// is switched off until the next power-up and Bluetooth has the radio alone.
//
// There is no password on /update: anyone on the home network could send
// firmware. That matches how WLED behaved on the same network.
#pragma once

// Prints like printf, to the serial port and to the log kept for /log.
// Safe to call from the Bluetooth task as well as from loop().
void say(const char* format, ...) __attribute__((format(printf, 1, 2)));

void netBegin();
void netLoop();  // call every pass through loop()

// One line describing the Bluetooth link, for the status page. Defined in main.cpp.
const char* linkStatusText();
