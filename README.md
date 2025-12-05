# DCF77 Transmitter for ESP32

[![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub stars](https://img.shields.io/github/stars/SensorsIot/DCF77-Transmitter-for-ESP32)](https://github.com/SensorsIot/DCF77-Transmitter-for-ESP32/stargazers)
[![YouTube](https://img.shields.io/badge/YouTube-Video-red?logo=youtube)](https://youtu.be/r2UAmBLBBRM)

![ESP32](https://img.shields.io/badge/ESP32-Supported-green?logo=espressif)
![Arduino](https://img.shields.io/badge/Arduino-Compatible-00979D?logo=arduino)
![77.5kHz](https://img.shields.io/badge/DCF77-77.5kHz-blue)

Simulate a DCF77 time signal transmitter using an ESP32. Sync your radio-controlled clocks without receiving the actual DCF77 signal.

📺 **Video Tutorial:** https://youtu.be/r2UAmBLBBRM

## 🔌 Wiring

| Function | GPIO |
|----------|------|
| Antenna | GPIO 15 |
| LED | GPIO 5 |

> 💡 **Tip:** Connect antenna from GPIO 15 to ground with a 1kΩ resistor to limit transmitting power. A tuned ferrite antenna reaches ~3 meters.

## ⚙️ Configuration

### 📶 WiFi Credentials

Create a file named `credentials.h` in the sketch folder:

```cpp
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

const char* ssid = "your-wifi-name";
const char* password = "your-wifi-password";

#endif
```

### 🌍 Time Zone

Edit the time zone in `DFC77_ESP32.ino`:

```cpp
const char* ntpServer = "pool.ntp.org";
const char* TZ_INFO = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
```

Find your time zone: https://remotemonitoringsystems.ca/time-zone-abbreviations.php

### ⏰ Continuous vs Scheduled Mode

By default, the transmitter runs on a schedule to save power. To transmit continuously:

```cpp
#define CONTINUOUSMODE  // Uncomment to transmit all the time
```

## 💡 Troubleshooting

### Deep Sleep NTP Sync Issue

If time drifts after waking from deep sleep, add this to `setup()` to force NTP sync:

```cpp
struct timeval tv;
tv.tv_sec = -1;
settimeofday(&tv, NULL);
```

### ESP32-C3 Compatibility

The ESP32-C3 requires amplitude modulation (15% signal level for "1" bits). Standard PWM won't work correctly. You'll need to use two pins with different attenuation levels to achieve proper AM modulation.

## 📦 Dependencies

| Library | Description |
|---------|-------------|
| WiFi.h | Built-in ESP32 WiFi |
| Ticker.h | Built-in timer library |
| Time.h | Time functions |
