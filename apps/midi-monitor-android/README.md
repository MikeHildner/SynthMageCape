# MIDI Monitor (Android)

Connects to a Bluetooth LE MIDI device and shows what it sends, live: the raw
packets as hex, the decoded MIDI messages, and which notes are currently held.

It was written to find out what a Korg Kronos sends through a CME WIDI Master
before writing ESP32 firmware that listens to the same data, and as a way to
learn Android and the BLE MIDI protocol. The code is commented with that in mind.

## Status

| Part | State |
|---|---|
| Scan for BLE devices, optional BLE MIDI filter | Tested on a phone |
| Message log, held-notes panel, demo data | Tested on a phone |
| Connecting: failure paths (not connectable, no MIDI service, pairing refused) | Tested on a phone |
| Connecting to a real BLE MIDI device and receiving from it | Works with a CME WIDI Master on a Korg Kronos |

Not built: tempo from MIDI clock (ticks are only counted) and chord naming.

## What the Kronos sends

Observed on 15 Nov 2026 with a CME WIDI Master on the Kronos MIDI OUT, as
reported from the app's screen. This is the input the cape firmware has to handle.

| Action | What arrives |
|---|---|
| Key press | Note On with velocity |
| Key release | A real Note Off with a release velocity (not Note On with velocity 0) |
| Sustain pedal | CC 64 with continuous values, not just 0 and 127 (half-damper). Treat 64 and above as down. |
| Joystick up | CC 1 (mod wheel), 0 to 127, back to 0 on release |
| Joystick down | CC 2, 0 to 127, back to 0 on release |
| Joystick left and right | Pitch bend, full range (-8192 to +8191), back to 0 on release |
| Ribbon | CC 16, 0 to 127; jumps to 64 (centre) when the finger lifts |
| Changing program | A burst: System Exclusive, bank select (CC 0, CC 32), Program Change, then CC 10, 80, 65 and 67 and more System Exclusive |
| Nothing at all | MIDI clock, continuously, even with the sequencer stopped; it follows the tempo setting |

- The constant clock means packets arrive all the time (48 ticks a second at
  120 BPM). "Data is arriving" does not mean "someone is playing", and the
  Kronos's tempo is always available.
- No packet has been flagged as a problem, so the adapter's packets match the
  format described below for everything played so far.
- The joystick, ribbon release, program change and tempo rows were added on
  17 Nov 2026 from the cape firmware's serial monitor (`firmware/cape-midi`),
  which uses a C++ port of this app's parser. That session also had no packet
  flagged as a problem.
- Not yet checked: aftertouch, and whether active sensing is sent.

### Control surface faders send System Exclusive, and a burst of it stalls the adapter

Captured on 21 Nov 2026 from the phone's log (`adb logcat -s MidiMonitor`)
while one control-surface fader was moved, with the Kronos in its normal mode.

- Each step of the fader is one 14-byte Korg message, with the position
  (00 to 7F) in the last byte before the F7:

      F0 42 30 68 43 04 07 00 02 00 00 00 vv F7

- The WIDI Master delivered this as bursts of five 20-byte packets about every
  80 ms. Messages are split across packets exactly as the parser expects (a
  continuation packet is a header byte followed directly by data), which
  happened 80 times without a problem.
- It cannot keep up. Fader positions are skipped between bursts, and 18 times a
  burst ended with a message missing its F7, followed by a fresh F0. That is
  what the app reports as "System Exclusive cut short by status F0"; the data
  was lost before it reached the phone.
- While this was going on the MIDI clock stopped arriving, and it did not come
  back afterwards: the connection stayed up, but no packet of any kind arrived
  for more than two minutes after the fader stopped. Before the fader was
  touched, clock ticks arrived every 20.8 ms (120 BPM) without a gap.

Reconnecting from the app brought the stream back.

The same fader with the control surface in External mode (EXT), captured the
same day:

- It sends Control Change 91 on channel 1, `B0 5B vv`, with `vv` covering the
  full 0 to 127. One message per 5-byte packet, 189 of them in a 2.4 second
  sweep down and back up.
- The clock kept its rate throughout, nothing was flagged as a problem, and
  the stream carried on afterwards.
- In Audio mode the fader sent nothing over MIDI at all.
- Pressing a CONTROL ASSIGN button, or changing mode or program, sends one or
  two short System Exclusive messages. Those arrived intact and did no harm.

For the cape this means: do not send control-surface System Exclusive through
the adapter. Put the control surface in External mode so the fader sends a
Control Change (three bytes), and consider switching System Exclusive
transmission off on the Kronos so that touching a control on stage cannot
stall the stream.

## Requirements

- Android 13 or later (API level 33), with Bluetooth LE. A phone emulator cannot
  do Bluetooth, so this needs a real phone.
- To build: Android Studio (which brings the JDK and the Android SDK). The
  project pins Android Gradle plugin 9.0.0 and Gradle 9.2.1.

## Build and install

From this folder, with the phone connected by USB and USB debugging allowed:

```
gradlew.bat installDebug
```

Other useful tasks:

```
gradlew.bat :midi:test        run the parser and held-notes unit tests on the PC
gradlew.bat assembleDebug     build app\build\outputs\apk\debug\app-debug.apk without installing
```

Or open this folder (the one containing `settings.gradle.kts`) in Android Studio
and press Run.

## Use

1. Open **MIDI Monitor** and allow the Bluetooth permission when asked.
2. Plug the BLE MIDI adapter into the instrument's MIDI OUT and power it on.
   Not MIDI THRU: THRU also powers the adapter, so it scans and connects
   normally, but it only repeats what arrives at MIDI IN and carries none of
   your playing.
3. Tap **Scan**. With **BLE MIDI only** on, only devices that advertise the BLE
   MIDI service are listed. Turn it off to see every BLE device nearby.
4. Tap the device. The status line under the title steps through connecting,
   discovering services and subscribing, and ends at **Listening**.
5. Play. Each packet appears at the top of the list with its hex above the
   decoded messages. The panel above the list shows the keys that are down, the
   notes ringing on the sustain pedal, and the pedal state.

**Pause** freezes the list so it can be read (the counters and the held-notes
panel stay live). **Clear** empties the list and resets the held notes. **Hex**
hides or shows the raw bytes. **Back** disconnects.

MIDI clock ticks and active sensing are counted in the summary line, not listed.

**Demo** on the scan screen plays a short scripted performance through the same
parser and screen, with no device needed.

If the device does not show up in a scan: a BLE device stops advertising while
something is connected to it, so check that it is not already connected to
another phone, computer or adapter.

## Bluetooth permissions

Android shows both of these to the user as one prompt, "Nearby devices".

| Permission | Why the app needs it |
|---|---|
| `BLUETOOTH_SCAN` | To find nearby BLE devices. Declared with `neverForLocation`, a promise that scan results are not used to work out where the phone is; because of it the app needs no location permission. |
| `BLUETOOTH_CONNECT` | To connect to the chosen device and receive data from it, and to ask the user to switch Bluetooth on. |

The app uses no network access and stores nothing.

## Layout

```
midi/   Pure Kotlin, no Android. Unit-tested on the PC.
        BleMidiParser    BLE MIDI packet -> MIDI messages with timestamps
        MidiMessage      one type per MIDI message
        HeldNotes        which notes are sounding, including the sustain pedal
        BleMidiEncoder   messages -> packet (demo data and round-trip tests)
app/    The Android app (Jetpack Compose).
        ble/BleScanner          scanning, as a Flow
        ble/BleMidiConnection   GATT connection and notifications, as a Flow
        ScanViewModel, MonitorViewModel   screen state
        ui/                     the screens
```

## The protocol in brief

BLE MIDI is one GATT service with one characteristic:

- Service `03B80E5A-EDE8-4B33-A751-6CE34EC4C700`
- Characteristic `7772E5DB-3868-4112-A1A9-F2669D106BF3` (read it once after
  connecting, then subscribe for notifications)

Each notification is a packet: a header byte carrying the high 6 bits of a
13-bit millisecond timestamp, then MIDI messages, each preceded by a byte
carrying the low 7 bits. `80 80 90 3C 7F` is header, timestamp, Note On, middle
C, velocity 127. The tests in `midi/src/test` are worked examples of every case
the parser handles.

The official BLE MIDI specification is available only to MIDI Association
members and was not consulted. The format here was taken from the Android
documentation and from vendor and open-source implementations, and one rule
(running status does not carry across packets) is a conservative reading that
still needs checking against real hardware. Packets that break a rule are shown
in the log as problems rather than guessed at.
