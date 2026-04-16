# LVGL Weather Station

A weather station project for ESP32 using LVGL graphics library and TFT display with touchscreen functionality.

## Features

- Real-time weather data display from Open-Meteo API
- Touchscreen interface with LVGL
- Temperature charts and forecasts
- Current, minimum, and maximum temperature display
- Interactive controls (slider, switch, refresh button)

## Hardware Requirements

- ESP32 microcontroller
- TFT display compatible with TFT_eSPI library
- FT6336 touchscreen controller
- WiFi connectivity

### Pin Configuration

- TOUCH_SDA: 25
- TOUCH_SCL: 26
- TOUCH_RST: 33
- TOUCH_INT: 35

## Software Requirements

### Libraries

- LVGL v9.2 (by kisvegabor)
- TFT_eSPI (by Bodmer)
- bb_captouch
- WiFi
- HTTPClient
- ArduinoJson

### Arduino IDE Setup

1. Install Arduino IDE
2. Add ESP32 board support
3. Install required libraries via Library Manager
4. Configure TFT_eSPI and LVGL according to the instructions at:
   - https://RandomNerdTutorials.com/cyd-lvgl/
   - https://RandomNerdTutorials.com/esp32-tft-lvgl/

## Installation

1. Clone or download this repository
2. Open `lvgl_weatherStation.ino` in Arduino IDE
3. Update WiFi credentials in the code:
   ```cpp
   const char* WIFI_SSID = "Your_SSID";
   const char* WIFI_PASS = "Your_Password";
   ```
4. Update location coordinates in `fetchWeather()` function if needed
5. Upload the code to your ESP32

## Usage

1. Power on the ESP32
2. The device will connect to WiFi and fetch initial weather data
3. Use the touchscreen to interact with the interface:
   - View current temperature and forecast
   - Use the slider and switch for controls
   - Press "Actualiser" button to refresh weather data

## API Configuration

The project uses Open-Meteo API for weather data. The current configuration fetches:
- Current weather temperature
- Daily min/max temperatures
- Hourly weather codes for forecast

Location is set to latitude=44.987, longitude=3.916 (adjust as needed).

## Customization

- Modify weather symbols in `weatherSymbol()` function
- Adjust display colors and layout in `lv_create_main_gui()`
- Change temperature history data in `temp_history[]` array

## Troubleshooting

- Ensure correct LVGL and TFT_eSPI configuration files are used
- Check touchscreen calibration and orientation
- Verify WiFi connection and API access
- Monitor serial output for debug information

## License

This project is open source. Please check individual library licenses for usage terms.