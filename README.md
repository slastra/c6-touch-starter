# ESP32-C6 Touch Starter

A production-ready starter template for the ESP32-C6-Touch-LCD-1.47 development board from Waveshare. This template provides a clean, modular foundation for building ESP32-C6 touch LCD applications with modern architecture and best practices built-in.

![ESP32-C6-Touch-LCD-1.47](https://www.waveshare.com/img/devkit/ESP32-C6-Touch-LCD-1.47/ESP32-C6-Touch-LCD-1.47-1.jpg)

**Board Documentation**: https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47

## Hardware Specifications

- **MCU**: ESP32-C6 (32-bit RISC-V, up to 160MHz)  
- **Memory**: 512KB SRAM, 8MB Flash
- **Display**: 1.47" IPS LCD, 172×320 resolution, JD9853 controller
- **Touch**: Capacitive touch panel, AXS5106L controller
- **Connectivity**: Wi-Fi 6 (802.11ax), Bluetooth 5 (BLE)
- **Interfaces**: USB-C (USB Serial/JTAG), SPI, I2C, GPIO

## Features

- Full screen utilization (172×320 pixels)
- 7-tile swipeable interface with professional navigation
- Navigation menu on main tile for direct tile access
- Barcode scanning with auto-scan functionality and activity detection
- MQTT barcode resolution with product lookup and image display
- Product image display with RGB565 format and loading spinner
- HTTP image proxy server for SSL bypass and format conversion
- Touch input functionality with OTA update triggers
- Console logging via USB Serial/JTAG
- Visual feedback system (button color changes)
- Brightness control with interactive slider (dims to 2%)
- WiFi connectivity with automatic reconnection
- Real-time sensor visualization with continuous line charts
- Sensor optimization - tile-aware reading for better performance
- Battery monitoring with voltage display
- System status dashboard with comprehensive information
- OTA update functionality with progress tracking
- Dual partition scheme for safe firmware updates
- Modular tile architecture with individual file separation
- Production-ready codebase with proper error handling
- Security best practices with credential separation and .gitignore protection

## Quick Start

### 1. Prerequisites

- ESP-IDF v6.0-dev (supports ESP32-C6)
- Node.js (for barcode resolution server)
- USB-C cable for flashing and monitoring

### 2. Configure Credentials

**ESP32 WiFi Setup:**
```bash
# Copy the template and edit with your WiFi credentials
cp main/credentials.h.example main/credentials.h
# Edit main/credentials.h with your WiFi SSID and password
```

**Server API Setup (Optional - for barcode scanning):**
```bash
# Copy the template and edit with your API key
cp server/.env.example server/.env
# Edit server/.env with your BarcodeLookup API key from https://www.barcodelookup.com/api
```

### 3. Build and Flash

```bash
# Setup ESP-IDF environment
source ../esp-idf/export.sh  # For Bash shell
# OR: source ../esp-idf/export.fish  # For Fish shell

# Build and flash
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### 4. Start Barcode Server (Optional)

```bash
cd server/
npm install
npm start
```

## Configuration

### WiFi Settings
Edit `main/credentials.h` (created from template):
```c
#define CONFIG_WIFI_SSID            "Your_WiFi_Network"
#define CONFIG_WIFI_PASSWORD        "Your_WiFi_Password"
```

### Barcode Server Settings  
Edit `server/.env` (created from template):
```env
BARCODELOOKUP_API_KEY=your_api_key_here
PORT=3000
```

### OTA Updates
Update the firmware URL in `main/app_config.h`:
```c
#define OTA_UPDATE_URL "https://github.com/your-repo/releases/latest/download/firmware.bin"
```

## User Interface

### 7-Tile Swipeable Interface:
1. **Settings** (0): System status table + brightness control slider
2. **Main** (1): Navigation menu with direct tile access + title and version  
3. **Updates** (2): WiFi connectivity + OTA update functionality
4. **System Info** (3): Hardware details + IP address display
5. **Accelerometer** (4): Real-time 3-axis acceleration charts
6. **Gyroscope** (5): Real-time 3-axis rotation rate charts  
7. **Barcode Scanner** (6): Auto-scan barcode reading with activity detection

### Navigation Options:
- **Swipe left/right** to navigate between tiles
- **Use navigation menu** on main tile for direct tile access
- **Touch interactions** with visual feedback

## Architecture

```
main/
├── app_config.h          # Constants & configuration
├── credentials.h         # WiFi credentials (not tracked in git)
├── main.c               # Clean coordination layer (58 lines)
├── barcode_manager.c    # UART barcode scanner integration
├── ui/                  # Display & interface
│   ├── ui_manager.c     # Main UI coordinator
│   ├── ui_components.c  # Buttons, labels, progress bars
│   ├── ui_theme.c       # Red/black color scheme
│   └── tiles/           # Modular tile system
├── network/             # Connectivity
│   ├── wifi_manager.c   # WiFi management
│   └── ota_manager.c    # OTA updates
└── components/
    └── esp_bsp/         # Board support package

server/
├── barcode-resolver.js  # MQTT barcode resolution service
├── .env                 # API keys (not tracked in git)
└── package.json         # Node.js dependencies
```

## Development

### Build Environment
- **Target**: esp32c6
- **Toolchain**: RISC-V GCC  
- **LVGL**: v8.4.0
- **Flash Size**: 8MB (configured for larger applications)
- **Partition Scheme**: Custom OTA layout with dual 3MB app slots

### USB Serial/JTAG Setup
- **Port**: `/dev/ttyACM0` (Linux)
- **Flash method**: UART (via ESP-IDF extension)
- **Baud rate**: 115200
- **Console output**: Real-time logging and events

## Security

This project implements security best practices:

- **Credentials separated** from source code
- **Template files** for easy setup
- **Comprehensive .gitignore** prevents credential commits
- **Environment variables** for server API keys
- **HTTPS OTA updates** with certificate validation

## Troubleshooting

### Build Issues
- Ensure all managed components are installed
- Check ESP-IDF version (v6.0-dev required for ESP32-C6)
- Verify 8MB flash configuration

### WiFi Issues  
- Check credentials in `main/credentials.h`
- Verify network connectivity and signal strength

### Display Issues
- Gap setting must be (34, 0) in bsp_display.c  
- Ensure color inversion is enabled

### OTA Issues
- Enable certificate bundle in menuconfig
- Check GitHub repository and release URL accessibility
- Verify dual OTA partition table configuration

## License

This project is provided as a starter template for ESP32-C6 development.

## Support

- **Hardware Documentation**: https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47
- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **LVGL Documentation**: https://docs.lvgl.io/