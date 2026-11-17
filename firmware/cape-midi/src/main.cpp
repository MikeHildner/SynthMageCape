// Step 3 of the MIDI firmware: first light.
//
// The board scans for a Bluetooth MIDI device (the CME WIDI Master in the
// Kronos MIDI OUT), connects, and prints what is played to the serial port
// (the COM socket, 115200 baud). No cape LEDs yet: the board's own LED answers
// the keys, so it can sit beside the keyboard on any USB power.
//
// The board's own LED, dimly:
//     blinking blue   scanning
//     steady blue     connecting
//     faint green     connected, nothing played
//     warm flash      a key was struck; brighter the harder it was struck
//     low warm glow   keys are being held
#include <Arduino.h>
#include <NimBLEDevice.h>

#include "BleMidiParser.h"
#include "ParserSelfTest.h"

const uint8_t ONBOARD_LED_PIN = 48;

// Brightness ceiling, 0-255. Nothing in this firmware may exceed it.
// The cape is for a dark stage: less is more.
const uint8_t MAX_BRIGHTNESS = 16;

// The one service and one characteristic that make up Bluetooth MIDI.
const NimBLEUUID MIDI_SERVICE("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
const NimBLEUUID MIDI_CHARACTERISTIC("7772E5DB-3868-4112-A1A9-F2669D106BF3");

const uint32_t SCAN_TIME_MS = 5000;

enum class LinkState { Scanning, Connecting, Connected };
volatile LinkState linkState = LinkState::Scanning;

// Bluetooth callbacks run on the Bluetooth task, not in loop(). They do as
// little as possible: set a flag, or put a message on this queue for loop().
QueueHandle_t midiQueue;
const NimBLEAdvertisedDevice* foundDevice = nullptr;
volatile bool connectRequested = false;
volatile uint32_t clockTicks = 0;      // MIDI clock, 24 per quarter note
volatile uint32_t droppedMessages = 0; // queue was full

BleMidiParser parser;

// ---------------------------------------------------------------- LED

// r, g, b are 0-255 "intent"; the ceiling is applied here and nowhere else.
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  static uint32_t last = 0xFFFFFFFF;
  const uint32_t packed = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (packed == last) return;
  last = packed;
  // Rounded up, so the faintest intent still lights the LED at level 1.
  rgbLedWrite(ONBOARD_LED_PIN, (r * MAX_BRIGHTNESS + 254) / 255, (g * MAX_BRIGHTNESS + 254) / 255,
              (b * MAX_BRIGHTNESS + 254) / 255);
}

// ---------------------------------------------------------------- note light

bool keyDown[128];
uint8_t keysDown = 0;
float flashLevel = 0;  // 0..1: set by a Note On to how hard the key was struck, then fades
float glowLevel = 0;   // 0..1: stays up while any key is held

const float FLASH_FADE_MS = 180;  // the flash falls to about a third in this time
const float HELD_GLOW = 0.30f;    // held keys glow at this fraction of the strike

void releaseAllKeys() {
  memset(keyDown, 0, sizeof(keyDown));
  keysDown = 0;
  flashLevel = 0;
  glowLevel = 0;
}

// Updates the note light from one MIDI message. Runs in loop().
void noteLight(const MidiMessage& m) {
  const uint8_t note = m.data1 & 0x7F;
  if (m.type() == 0x90 && m.data2 > 0) {
    if (!keyDown[note]) {
      keyDown[note] = true;
      keysDown++;
    }
    flashLevel = m.data2 / 127.0f;
    glowLevel = HELD_GLOW * flashLevel;
  } else if (m.type() == 0x80 || m.type() == 0x90) {  // Note Off, or Note On with velocity 0
    if (keyDown[note]) {
      keyDown[note] = false;
      keysDown--;
    }
    if (keysDown == 0) glowLevel = 0;
  }
}

void showLight() {
  static uint32_t lastFadeMs = 0;
  const uint32_t now = millis();
  if (now != lastFadeMs) {
    flashLevel *= expf(-(float)(now - lastFadeMs) / FLASH_FADE_MS);
    lastFadeMs = now;
  }

  switch (linkState) {
    case LinkState::Scanning:
      if ((millis() / 500) % 2) setLed(0, 0, 255); else setLed(0, 0, 0);
      return;
    case LinkState::Connecting:
      setLed(0, 0, 255);
      return;
    case LinkState::Connected:
      break;
  }

  const float level = max(flashLevel, glowLevel);
  if (level < 0.02f) {
    setLed(0, 16, 0);  // faint green: connected, nothing played
  } else {
    setLed((uint8_t)(255 * level), (uint8_t)(150 * level), (uint8_t)(50 * level));  // warm white
  }
}

// ---------------------------------------------------------------- MIDI in

// Called by the parser for each message in a packet (on the Bluetooth task).
void onMidiMessage(const MidiMessage& message, void*) {
  if (message.status == 0xF8) {  // clock: far too frequent to queue, just count
    clockTicks = clockTicks + 1;
    return;
  }
  if (message.status == 0xFE) return;  // active sensing: a keep-alive, nothing to show
  if (xQueueSend(midiQueue, &message, 0) != pdTRUE) {
    droppedMessages = droppedMessages + 1;
  }
}

// Called by the Bluetooth stack for each notification from the WIDI Master.
void onMidiPacket(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  const uint8_t problems = parser.parse(data, length, onMidiMessage, nullptr);
  if (problems > 0) {
    Serial.printf("  ? packet problem: %s. Bytes:", parser.lastProblem());
    for (size_t i = 0; i < length; i++) Serial.printf(" %02X", data[i]);
    Serial.println();
  }
}

// ---------------------------------------------------------------- Bluetooth

void startScan() {
  linkState = LinkState::Scanning;
  NimBLEDevice::getScan()->start(SCAN_TIME_MS, false, true);
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    if (!device->isAdvertisingService(MIDI_SERVICE)) return;
    Serial.printf("Found a Bluetooth MIDI device: \"%s\"  address %s  signal %d dBm\n",
                  device->getName().c_str(), device->getAddress().toString().c_str(), device->getRSSI());
    NimBLEDevice::getScan()->stop();
    foundDevice = device;
    connectRequested = true;
  }

  void onScanEnd(const NimBLEScanResults&, int) override {
    // Nothing found this round (or the scan was stopped to connect).
    if (linkState == LinkState::Scanning && !connectRequested) startScan();
  }
} scanCallbacks;

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int reason) override {
    Serial.printf("Disconnected (reason %d). Scanning again.\n", reason);
    parser.reset();
    startScan();
  }
} clientCallbacks;

// Connects to foundDevice and subscribes to its MIDI. Runs in loop().
bool connectToMidiDevice() {
  NimBLEClient* client = NimBLEDevice::getDisconnectedClient();
  if (client == nullptr) client = NimBLEDevice::createClient();
  if (client == nullptr) {
    Serial.println("Could not create a Bluetooth client.");
    return false;
  }
  client->setClientCallbacks(&clientCallbacks, false);
  // Connection interval 15 ms (12 x 1.25 ms): the short interval Bluetooth MIDI
  // asks for, so a note arrives quickly. Supervision timeout 1.5 s.
  client->setConnectionParams(12, 12, 0, 150);
  client->setConnectTimeout(5000);

  if (!client->connect(foundDevice)) {
    Serial.println("Connect failed.");
    return false;
  }

  NimBLERemoteService* service = client->getService(MIDI_SERVICE);
  NimBLERemoteCharacteristic* midi = service ? service->getCharacteristic(MIDI_CHARACTERISTIC) : nullptr;
  if (midi == nullptr) {
    Serial.println("Connected, but the MIDI service or characteristic is missing.");
    client->disconnect();
    return false;
  }

  // Bluetooth MIDI asks the listener to read the characteristic once first.
  if (midi->canRead()) midi->readValue();

  if (!midi->canNotify() || !midi->subscribe(true, onMidiPacket)) {
    Serial.println("Connected, but could not subscribe to MIDI notifications.");
    client->disconnect();
    return false;
  }

  Serial.printf("Connected and listening. Connection interval %.2f ms.\n", client->getConnInfo().getConnInterval() * 1.25);
  return true;
}

// ---------------------------------------------------------------- printing

const char* const PITCH_CLASSES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Same convention as the Android monitor: note 60 is C4.
void noteName(uint8_t note, char* out, size_t size) {
  snprintf(out, size, "%s%d", PITCH_CLASSES[note % 12], note / 12 - 1);
}

const char* controllerName(uint8_t controller) {
  switch (controller) {
    case 1: return " (mod, joystick up)";
    case 7: return " (volume)";
    case 16: return " (ribbon)";
    case 64: return " (sustain pedal)";
    default: return "";
  }
}

void printMessage(const MidiMessage& m) {
  char name[6];
  switch (m.type()) {
    case 0x90:
      if (m.data2 > 0) {
        noteName(m.data1, name, sizeof(name));
        Serial.printf("Note On   %-4s (%3u)  velocity %3u  ch %u\n", name, m.data1, m.data2, m.channel() + 1);
        break;
      }
      // Note On with velocity 0 means Note Off.
      [[fallthrough]];
    case 0x80:
      noteName(m.data1, name, sizeof(name));
      Serial.printf("Note Off  %-4s (%3u)\n", name, m.data1);
      break;
    case 0xA0:
      Serial.printf("Key pressure   note %u  value %u\n", m.data1, m.data2);
      break;
    case 0xB0:
      Serial.printf("Control %3u%s  value %3u\n", m.data1, controllerName(m.data1), m.data2);
      break;
    case 0xC0:
      Serial.printf("Program change %u\n", m.data1);
      break;
    case 0xD0:
      Serial.printf("Channel pressure %u\n", m.data1);
      break;
    case 0xE0:
      // 14 bits, least significant first, centred on zero.
      Serial.printf("Pitch bend %+d\n", ((m.data2 << 7) | m.data1) - 8192);
      break;
    case 0xF0:
      Serial.println("System Exclusive (contents skipped)");
      break;
    case 0xFA:
      Serial.println("Start");
      break;
    case 0xFB:
      Serial.println("Continue");
      break;
    case 0xFC:
      Serial.println("Stop");
      break;
    default:
      Serial.printf("Other message: status %02X  data %u %u\n", m.status, m.data1, m.data2);
      break;
  }
}

// The Kronos sends MIDI clock all the time, 24 ticks per quarter note.
// Count ticks over a few seconds and print the tempo when it changes.
// Bluetooth delivers ticks in small bursts, so a reading wobbles by a beat or
// so; only a change of 3 BPM or more is reported.
void reportTempo() {
  static uint32_t windowStart = 0;
  static uint32_t ticksAtStart = 0;
  static int lastBpm = -1;
  static bool firstWindow = true;
  const uint32_t WINDOW_MS = 8000;

  const uint32_t now = millis();
  const uint32_t ticks = clockTicks;

  // While not connected, keep restarting the window, so the first reading
  // after connecting covers a full window and not part of one.
  if (linkState != LinkState::Connected) {
    windowStart = now;
    ticksAtStart = ticks;
    lastBpm = -1;
    firstWindow = true;
    return;
  }
  if (now - windowStart < WINDOW_MS) return;

  const uint32_t counted = ticks - ticksAtStart;
  const int bpm = (int)((counted * 60000.0f) / (24.0f * (now - windowStart)) + 0.5f);
  windowStart = now;
  ticksAtStart = ticks;

  // The clock takes a moment to start flowing after connecting, so the first
  // window reads low. Skip it.
  if (firstWindow) {
    firstWindow = false;
    return;
  }
  if (abs(bpm - lastBpm) >= 3) {
    if (bpm == 0) Serial.println("No MIDI clock arriving.");
    else Serial.printf("MIDI clock: about %d BPM\n", bpm);
    lastBpm = bpm;
  }
}

// ---------------------------------------------------------------- main

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("Synth Mage cape, MIDI firmware, step 3: first light");

  runParserSelfTest();

  midiQueue = xQueueCreate(64, sizeof(MidiMessage));

  NimBLEDevice::init("SynthCape");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);
  scan->setInterval(100);
  scan->setWindow(100);
  scan->setActiveScan(true);

  Serial.println("Scanning for a Bluetooth MIDI device...");
  startScan();
}

void loop() {
  if (connectRequested) {
    connectRequested = false;
    linkState = LinkState::Connecting;
    showLight();
    if (connectToMidiDevice()) {
      linkState = LinkState::Connected;
    } else {
      startScan();
    }
  }

  MidiMessage message;
  while (xQueueReceive(midiQueue, &message, 0) == pdTRUE) {
    noteLight(message);  // light first, so printing never delays it
    printMessage(message);
  }
  // A dropped connection can swallow Note Offs; do not leave keys stuck down.
  if (linkState != LinkState::Connected && keysDown > 0) releaseAllKeys();

  static uint32_t reportedDrops = 0;
  if (droppedMessages != reportedDrops) {
    reportedDrops = droppedMessages;
    Serial.printf("  ! %lu messages dropped so far (queue full)\n", (unsigned long)reportedDrops);
  }

  reportTempo();
  showLight();
  delay(2);
}
