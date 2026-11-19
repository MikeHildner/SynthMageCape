#include "Net.h"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <stdarg.h>

#include "BuildStamp.h"
#include "Cape.h"
#include "secrets.h"

namespace {

const uint32_t WIFI_JOIN_TIMEOUT_MS = 15000;
const char* const NOT_SET = "TYPE-YOUR-WIFI-PASSWORD-HERE";

enum class WifiState { Off, Joining, Up };
WifiState wifiState = WifiState::Off;
uint32_t joinStartedMs = 0;

WebServer server(80);

// ---- the log kept for /log: a ring of the most recent characters

const size_t LOG_SIZE = 6000;
char logRing[LOG_SIZE];
size_t logNext = 0;       // where the next character goes
bool logWrapped = false;  // true once the ring has filled at least once
portMUX_TYPE logLock = portMUX_INITIALIZER_UNLOCKED;

void handleStatus() {
  char page[400];
  snprintf(page, sizeof(page),
           "Synth Mage cape, MIDI firmware\n"
           "built        %s\n"
           "running for  %lu s\n"
           "bluetooth    %s\n"
           "wifi signal  %d dBm\n"
           "free memory  %lu bytes\n"
           "brightness   ceiling %u of 255\n"
           "frame rate   %u a second\n",
           BUILD_STAMP, (unsigned long)(millis() / 1000), linkStatusText(), WiFi.RSSI(),
           (unsigned long)ESP.getFreeHeap(), MAX_BRIGHTNESS, capeFramesPerSecond());
  server.send(200, "text/plain", page);
}

void handleLog() {
  static char copy[LOG_SIZE + 1];
  size_t length = 0;
  portENTER_CRITICAL(&logLock);
  if (logWrapped) {
    memcpy(copy, logRing + logNext, LOG_SIZE - logNext);
    length = LOG_SIZE - logNext;
  }
  memcpy(copy + length, logRing, logNext);
  length += logNext;
  portEXIT_CRITICAL(&logLock);
  copy[length] = 0;
  server.send(200, "text/plain", copy);
}

// Called when the whole upload has arrived.
void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(500, "text/plain", "Update FAILED. The old firmware is still in place.\n");
    say("Update failed: %s\n", Update.errorString());
  } else {
    server.send(200, "text/plain", "Update accepted. Restarting.\n");
    say("Update accepted. Restarting.\n");
    delay(500);
    ESP.restart();
  }
}

// Called repeatedly as pieces of the upload arrive.
void handleUpdateData() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    say("Update starting over Wi-Fi...\n");
    capeBlank();  // lights out while the new firmware is written
    Update.begin();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else {
    Update.abort();
  }
}

}  // namespace

void say(const char* format, ...) {
  char text[240];
  va_list arguments;
  va_start(arguments, format);
  int length = vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);
  if (length <= 0) return;
  if ((size_t)length >= sizeof(text)) length = sizeof(text) - 1;

  Serial.write((const uint8_t*)text, length);

  portENTER_CRITICAL(&logLock);
  for (int i = 0; i < length; i++) {
    logRing[logNext] = text[i];
    logNext++;
    if (logNext == LOG_SIZE) {
      logNext = 0;
      logWrapped = true;
    }
  }
  portEXIT_CRITICAL(&logLock);
}

void netBegin() {
  if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_PASSWORD, NOT_SET) == 0) {
    say("Wi-Fi: not set up (see include/secrets.h). Bluetooth only.\n");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);  // required when Bluetooth shares the radio
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiState = WifiState::Joining;
  joinStartedMs = millis();
  say("Wi-Fi: joining the home network...\n");
}

void netLoop() {
  switch (wifiState) {
    case WifiState::Off:
      return;

    case WifiState::Joining:
      if (WiFi.status() == WL_CONNECTED) {
        server.on("/", HTTP_GET, handleStatus);
        server.on("/log", HTTP_GET, handleLog);
        server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateData);
        server.begin();
        wifiState = WifiState::Up;
        say("Wi-Fi: joined. Address %s\n", WiFi.localIP().toString().c_str());
      } else if (millis() - joinStartedMs > WIFI_JOIN_TIMEOUT_MS) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifiState = WifiState::Off;
        say("Wi-Fi: home network not found. Wi-Fi off; Bluetooth has the radio.\n");
      }
      return;

    case WifiState::Up:
      server.handleClient();
      return;
  }
}
