// Step 4 of the MIDI firmware: the lapels.
//
// The board scans for a Bluetooth MIDI device (the CME WIDI Master in the
// Kronos MIDI OUT), connects, and turns what is played into light on the
// cape's lapels and collar (see Cape.h). It also prints every message to the serial
// port (the COM socket, 115200 baud) and keeps the latest for the log page.
//
// At home it also joins Wi-Fi, for updates and the log (see Net.h).
//
// What the lights mean:
//     hem row blinking blue     searching for the WIDI Master (the collar
//                               bottoms blink with it)
//     hem row steady blue       connecting
//     one green sweep upward    connected (up the lapels, then round the collar)
//     ripple from a row         a key was struck: the row is the pitch, the
//                               brightness is how hard it was struck, and the
//                               colour is the note name (see LOOK in Cape.h)
//     low glow on a row         that key is being held, or was released with
//                               the sustain pedal down (deeper pedal, brighter)
//     soft wash on both lapels  an organ key is held (see ORGAN_FROM_NOTE)
//     collar glowing            notes are sounding: each has its own band of
//                               the collar, in its colour and at its level
//     colours swinging          joystick up (controller 1, vibrato): the
//                               further up, the wider the swing
#include <Arduino.h>
#include <NimBLEDevice.h>

#include "BleMidiParser.h"
#include "BuildStamp.h"
#include "Cape.h"
#include "Net.h"
#include "ParserSelfTest.h"

// The one service and one characteristic that make up Bluetooth MIDI.
const NimBLEUUID MIDI_SERVICE("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
const NimBLEUUID MIDI_CHARACTERISTIC("7772E5DB-3868-4112-A1A9-F2669D106BF3");

const uint32_t SCAN_TIME_MS = 5000;

volatile LinkState linkState = LinkState::Scanning;

// Bluetooth callbacks run on the Bluetooth task, not in loop(). They do as
// little as possible: set a flag, or put a message on this queue for loop().
QueueHandle_t midiQueue;
const NimBLEAdvertisedDevice* foundDevice = nullptr;
volatile bool connectRequested = false;
volatile uint32_t clockTicks = 0;      // MIDI clock, 24 per quarter note
volatile uint32_t droppedMessages = 0; // queue was full

BleMidiParser parser;

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
    say("  ? packet problem: %s. Bytes:", parser.lastProblem());
    for (size_t i = 0; i < length; i++) say(" %02X", data[i]);
    say("\n");
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
    say("Found a Bluetooth MIDI device: \"%s\"  address %s  signal %d dBm\n",
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
    say("Disconnected (reason %d). Scanning again.\n", reason);
    parser.reset();
    startScan();
  }
} clientCallbacks;

// Connects to foundDevice and subscribes to its MIDI. Runs in loop().
bool connectToMidiDevice() {
  NimBLEClient* client = NimBLEDevice::getDisconnectedClient();
  if (client == nullptr) client = NimBLEDevice::createClient();
  if (client == nullptr) {
    say("Could not create a Bluetooth client.\n");
    return false;
  }
  client->setClientCallbacks(&clientCallbacks, false);
  // Connection interval 15 ms (12 x 1.25 ms): the short interval Bluetooth MIDI
  // asks for, so a note arrives quickly. Supervision timeout 1.5 s.
  client->setConnectionParams(12, 12, 0, 150);
  client->setConnectTimeout(5000);

  if (!client->connect(foundDevice)) {
    say("Connect failed.\n");
    return false;
  }

  NimBLERemoteService* service = client->getService(MIDI_SERVICE);
  NimBLERemoteCharacteristic* midi = service ? service->getCharacteristic(MIDI_CHARACTERISTIC) : nullptr;
  if (midi == nullptr) {
    say("Connected, but the MIDI service or characteristic is missing.\n");
    client->disconnect();
    return false;
  }

  // Bluetooth MIDI asks the listener to read the characteristic once first.
  if (midi->canRead()) midi->readValue();

  if (!midi->canNotify() || !midi->subscribe(true, onMidiPacket)) {
    say("Connected, but could not subscribe to MIDI notifications.\n");
    client->disconnect();
    return false;
  }

  say("Connected and listening. Connection interval %.2f ms.\n", client->getConnInfo().getConnInterval() * 1.25);
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
        say("Note On   %-4s (%3u)  velocity %3u  ch %u\n", name, m.data1, m.data2, m.channel() + 1);
        break;
      }
      // Note On with velocity 0 means Note Off.
      [[fallthrough]];
    case 0x80:
      noteName(m.data1, name, sizeof(name));
      say("Note Off  %-4s (%3u)\n", name, m.data1);
      break;
    case 0xA0:
      say("Key pressure   note %u  value %u\n", m.data1, m.data2);
      break;
    case 0xB0:
      say("Control %3u%s  value %3u\n", m.data1, controllerName(m.data1), m.data2);
      break;
    case 0xC0:
      say("Program change %u\n", m.data1);
      break;
    case 0xD0:
      say("Channel pressure %u\n", m.data1);
      break;
    case 0xE0:
      // 14 bits, least significant first, centred on zero.
      say("Pitch bend %+d\n", ((m.data2 << 7) | m.data1) - 8192);
      break;
    case 0xF0:
      say("System Exclusive (contents skipped)\n");
      break;
    case 0xFA:
      say("Start\n");
      break;
    case 0xFB:
      say("Continue\n");
      break;
    case 0xFC:
      say("Stop\n");
      break;
    default:
      say("Other message: status %02X  data %u %u\n", m.status, m.data1, m.data2);
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
    if (bpm == 0) say("No MIDI clock arriving.\n");
    else say("MIDI clock: about %d BPM\n", bpm);
    lastBpm = bpm;
  }
}

// ---------------------------------------------------------------- main

const char* linkStatusText() {
  switch (linkState) {
    case LinkState::Scanning: return "searching for the WIDI Master";
    case LinkState::Connecting: return "connecting";
    default: return "connected, listening to the Kronos";
  }
}

void setup() {
  // Room for a burst of lines, so say() seldom has to skip one (see Net.cpp).
  Serial.setTxBufferSize(2048);
  Serial.begin(115200);
  delay(300);
  say("\n");
  say("Synth Mage cape, MIDI firmware. Built %s\n", BUILD_STAMP);

  capeBegin();
  runParserSelfTest();
  netBegin();

  midiQueue = xQueueCreate(64, sizeof(MidiMessage));

  NimBLEDevice::init("SynthCape");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);
  scan->setInterval(100);
  scan->setWindow(100);
  scan->setActiveScan(true);

  say("Scanning for a Bluetooth MIDI device...\n");
  startScan();
}

void loop() {
  if (connectRequested) {
    connectRequested = false;
    linkState = LinkState::Connecting;
    capeShow(linkState);
    if (connectToMidiDevice()) {
      linkState = LinkState::Connected;
    } else {
      startScan();
    }
  }

  MidiMessage message;
  while (xQueueReceive(midiQueue, &message, 0) == pdTRUE) {
    // Light first, so printing never delays it.
    if (message.type() == 0x90 && message.data2 > 0) {
      capeNoteOn(message.data1, message.data2);
    } else if (message.type() == 0x80 || message.type() == 0x90) {  // Note Off, or Note On with velocity 0
      capeNoteOff(message.data1);
    } else if (message.type() == 0xB0 && message.data1 == 64) {  // sustain pedal
      capePedal(message.data2);
    } else if (message.type() == 0xB0 && message.data1 == 1) {  // mod wheel: joystick up, vibrato
      capeModulation(message.data2);
    } else if (message.type() == 0xE0) {  // pitch bend: 14 bits, centred on 8192
      capeBend(((message.data2 << 7) | message.data1) - 8192);
    }
    printMessage(message);
  }
  // A dropped connection can swallow Note Offs; do not leave keys stuck down.
  if (linkState != LinkState::Connected) capeAllNotesOff();

  static uint32_t reportedDrops = 0;
  if (droppedMessages != reportedDrops) {
    reportedDrops = droppedMessages;
    say("  ! %lu messages dropped so far (queue full)\n", (unsigned long)reportedDrops);
  }

  reportTempo();
  capeShow(linkState);
  netLoop();
  delay(1);
}
