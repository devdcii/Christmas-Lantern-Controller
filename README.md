# Christmas Lantern Controller
### IoT-Based LED Lantern Control System

> A Flutter mobile app that controls an ESP32-powered Christmas lantern with 6 LED circles — featuring 10 light patterns, color selection, per-circle control, and pattern sequences over a direct WiFi connection.

---

## Overview

Christmas Lantern Controller is an iOS-style Flutter app that connects directly to an ESP32 Access Point (no router needed) to control 6 WS2812B LED strip circles. Users can set global patterns, assign individual patterns per circle, build custom pattern sequences, and choose from 8 colors — all in real time.

On startup, the ESP32 automatically runs a **Showcase Mode** that cycles through all 10 light patterns every 30 seconds with color cycling every 2 seconds, until the app takes control.

---

## Features

- **10 Light Patterns** — Static, Breathing, Rainbow, Chase, Twinkle, Wave, Fire, Christmas, Circle Wave, Alternating
- **Per-Circle Control** — select individual circles and assign different patterns to each
- **Pattern Sequences** — build a custom sequence of patterns that auto-cycle every 5 seconds per circle
- **Color Selection** — 8 color presets (Red, Orange, Yellow, Green, Blue, Purple, Pink, White)
- **Showcase Mode** — auto-cycles all 10 patterns on startup with color cycling
- **Auto Pilot Mode** — rainbow effect that kicks in when the app disconnects
- **Power Toggle** — turn all 6 circles on/off from the app
- **Live Connection Status** — real-time WiFi connection indicator
- **Web Control Panel** — browser-accessible dashboard at `http://192.168.4.1`

---

## Tech Stack

| Layer | Technology |
|---|---|
| Mobile App | Flutter 3.x (Dart) |
| UI Style | Cupertino (iOS-style) |
| HTTP Client | http package |
| Microcontroller | ESP32 |
| Firmware | Arduino (C++) |
| LED Library | FastLED |
| LED Strip | WS2812B |
| Communication | WiFi (ESP32 Access Point) |
| Data Format | JSON / REST |

---

## Project Structure

```
christmas-lantern/
├── app/                              # Flutter Mobile App
│   ├── lib/
│   │   └── main.dart                 # Full app (ESP32Service, screens, patterns)
│   └── pubspec.yaml
│
└── firmware/                         # ESP32 Arduino Code
    └── christmas_lantern/
        └── christmas_lantern.ino     # Main firmware file
```

---

## Hardware

### Components

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32 Dev Module | WiFi Access Point + LED controller |
| LED Strip | WS2812B (x6 strips) | Addressable RGB LEDs per circle |
| Power Supply | 5V (adequate amperage) | Powers LEDs + ESP32 |

### LED Circle Configuration

| Circle | ESP32 Pin | GPIO |
|---|---|---|
| Circle 1 | D2 | GPIO 2 |
| Circle 2 | D4 | GPIO 4 |
| Circle 3 | D16 | GPIO 16 |
| Circle 4 | D17 | GPIO 17 |
| Circle 5 | D18 | GPIO 18 |
| Circle 6 | D19 | GPIO 19 |

Each circle has **50 LEDs** (WS2812B, GRB color order).

> **Note:** WS2812B strips require a 300-500 ohm resistor on the data line and a 1000uF capacitor across the power supply to prevent power surge damage.

### ESP32 WiFi Setup

The ESP32 creates its own Access Point — no router required. The phone connects directly to it.

| Setting | Value |
|---|---|
| SSID | ChristmasLantern |
| Password | 12345678 |
| ESP32 IP | 192.168.4.1 |
| Max Connections | 4 |
| Channel | 1 |

### ESP32 HTTP API

Base URL: `http://192.168.4.1`

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Web control panel (browser) |
| `/test` | GET | Connection test + device info |
| `/api/status` | GET | Full system status + circle info + sequences |
| `/api/control` | POST | Power, pattern, brightness, showcase/autopilot toggle |
| `/api/color` | POST | Set global RGB color |
| `/api/circles` | POST | Toggle circle, set per-circle pattern |
| `/api/sequences` | POST | Set or clear pattern sequences per circle |

### Arduino IDE Setup

1. Install **Arduino IDE** and add ESP32 board support
   - Board Manager URL: `https://dl.espressif.com/dl/package_esp32_index.json`
2. Install required libraries via Library Manager:
   - `FastLED` by Daniel Garcia
   - `ArduinoJson` by Benoit Blanchon
3. Open `firmware/christmas_lantern/christmas_lantern.ino`
4. Update credentials if needed:
```cpp
const char* ap_ssid = "ChristmasLantern";
const char* ap_password = "12345678";
```
5. Update LED count or pin assignments if your wiring differs:
```cpp
#define LEDS_PER_CIRCLE 50
const int LED_PINS[NUM_CIRCLES] = {2, 4, 16, 17, 18, 19};
```
6. Select board: **ESP32 Dev Module**
7. Upload to ESP32
8. Open Serial Monitor at **115200 baud** to verify startup

### Light Patterns

| ID | Pattern | Description |
|---|---|---|
| 0 | Static Color | Solid fill with selected color |
| 1 | Breathing | Fade in/out effect |
| 2 | Rainbow | Full spectrum color cycle |
| 3 | Chase | Moving dot with trail |
| 4 | Twinkle | Random sparkling LEDs |
| 5 | Wave | Sine wave brightness effect |
| 6 | Fire | Random warm flicker |
| 7 | Christmas | Alternating red and green |
| 8 | Circle Wave | Wave synced across circles |
| 9 | Alternating | Circles alternate between 6 color pairs |

---

## Mobile App Setup

### Prerequisites

- Flutter SDK 3.0+
- Dart SDK 3.0+
- Android Studio or VS Code with Flutter extensions

### Installation

```bash
cd app
flutter pub get
flutter run
```

### Connecting to the Lantern

1. Power on the ESP32
2. On your phone, connect to WiFi: **ChristmasLantern** (password: `12345678`)
3. Open the app — the top bar turns green when connected
4. The ESP32 IP is fixed at `192.168.4.1` — no config needed

---

## App Screens

The app is a single screen with these sections:

| Section | Description |
|---|---|
| Connection Bar | Shows Connected / Connecting with live indicator |
| Power Toggle | ON/OFF button for all circles |
| LED Color | 8 color preset selector |
| Circle Selection | Tap circles 1–6 to select for individual control |
| Configure Sequence | Build a custom pattern sequence for selected circles |
| Light Patterns | 10-pattern grid — tap to apply globally or to selected circles |

---

## Dependencies

```yaml
dependencies:
  flutter:
    sdk: flutter
  cupertino_icons: ^1.0.8
  http: ^1.5.0
```

---

## Roadmap

- [ ] Custom RGB color picker
- [ ] Brightness slider control
- [ ] Save and load pattern presets
- [ ] Schedule on/off timer
- [ ] Multiple lantern support
- [ ] OTA firmware update from app
