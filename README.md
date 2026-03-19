# ESP32 WebSocket Temperature Monitor

An ESP32 Arduino project that reads temperature from a DS18B20 sensor and streams live readings to a web browser over WebSocket and Server-Sent Events (SSE). Readings are also logged to a CSV file on an SD card.

## Features

- Real-time temperature readings via WebSocket and SSE
- Web interface served from the SD card
- Temperature logging to `/temperature_log.csv` on the SD card
- NTP time-stamping of log entries
- REST endpoints to download or clear the CSV log

## Hardware Requirements

- ESP32 development board
- DS18B20 temperature sensor (connected to GPIO 4)
- SD card module (CS on GPIO 5)

## Dependencies

Managed via PlatformIO (`platformio.ini`):

| Library | Purpose |
|---|---|
| OneWire | 1-Wire bus communication |
| DallasTemperature | DS18B20 temperature sensor |
| Arduino_JSON | JSON serialization |
| NTPClient | Network time synchronization |
| ESPAsyncWebServer | Async HTTP & WebSocket server |
| AsyncTCP | Async TCP layer for ESP32 |

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository and open it in VS Code with the PlatformIO extension.
3. Update the Wi-Fi credentials in `src/main.cpp`:
   ```cpp
   const char *ssid     = "YOUR_SSID";
   const char *password = "YOUR_PASSWORD";
   ```
4. Update the `upload_port` in `platformio.ini` to match your serial port.
5. Copy the contents of the `data/` folder to the root of your SD card.
6. Build and upload with PlatformIO: `pio run --target upload`.
7. Open the Serial Monitor at **115200 baud** to find the device's IP address.
8. Navigate to the IP address in a browser.

## API Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/` | Serves the web UI (`index.html`) |
| GET | `/readings` | Returns current temperature as JSON |
| GET | `/download-csv` | Downloads the full temperature log |
| GET | `/clear-csv` | Clears the temperature log |
| WS | `/ws` | WebSocket – send `getReadings` to poll temperature |
| SSE | `/events` | Server-Sent Events stream (`new_readings` event) |
