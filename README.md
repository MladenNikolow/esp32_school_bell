# ESP32 School Bell

An automated school bell system built on ESP32-S3 with touch LCD interface and web-based configuration. This project enables scheduled ringing of school bells with flexible configuration options through both a touch screen interface and a web application.

## Overview

The ESP32 School Bell is an intelligent bell automation system designed for educational institutions. It provides reliable, automated bell ringing based on customizable schedules, with the flexibility to be controlled both locally via a touch screen and remotely through a web interface. The system ensures schools can maintain their daily schedules accurately while providing easy configuration options for administrators.

### Key Features

- **Automated Scheduling**: Configure bell schedules for different days and times
- **Dual Configuration Interface**: 
  - Local configuration via 4-inch touch LCD screen
  - Remote configuration through web interface (React-based)
- **WiFi Connectivity**: Supports both Access Point (AP) and Station (STA) modes
- **Manual Override**: Emergency and manual bell control options
- **Persistent Storage**: Schedules and settings stored in Non-Volatile Storage (NVS)
- **Web Interface**: React-based web application hosted on the ESP32 (from [esp32_school_bell_web](https://github.com/MladenNikolow/esp32_school_bell_web) submodule)
- **RESTful API**: HTTP API for remote control and configuration

## Hardware

### ESP32-S3-Touch-LCD-4

This project is designed for the [Waveshare ESP32-S3-Touch-LCD-4](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4) development board.

**Board Specifications:**
- **MCU**: ESP32-S3-WROOM-1-N4R8 module
  - Dual-core Xtensa LX7 processor
  - 4MB Flash, 8MB PSRAM
  - WiFi 802.11 b/g/n
  - Bluetooth 5.0 LE
- **Display**: 4-inch IPS LCD (480×480 pixels)
- **Touch**: Capacitive touch screen
- **Interfaces**: GPIO, I2C, SPI, UART, USB
- **Power**: USB Type-C or external power supply

## Technical Details

### Architecture

The project is structured using ESP-IDF (Espressif IoT Development Framework) with FreeRTOS as the underlying operating system. The codebase is modular, with separate libraries for each major component:

#### Core Components

- **AppTask**: Main application task coordinator
- **RingBell**: Bell control module (GPIO-based relay control)
- **WiFi_Manager**: WiFi connectivity management (AP/STA modes)
- **WebServer**: HTTP server with REST API and static file serving
  - React interface hosting
  - RESTful API endpoints for configuration
  - WebSocket support for real-time updates
- **FileSystem**: FAT filesystem management (React web files storage)
- **NVS**: Non-Volatile Storage for persistent configuration

#### Web Interface

The web interface is a separate React application developed in the [MladenNikolow/esp32_school_bell_web](https://github.com/MladenNikolow/esp32_school_bell_web) repository. The built React application (static HTML/JS/CSS files) is stored in the ESP32's FAT filesystem partition and served by the embedded web server.

**Features of the Web Interface:**
- Schedule management
- WiFi configuration
- Manual bell control
- System status monitoring
- Settings configuration

### Storage Partitioning

The ESP32 flash is partitioned as follows (from `partitions/esp32_flash_3mb_fatfs_1mb.csv`):

| Name | Type | SubType | Offset | Size | Purpose |
|------|------|---------|--------|------|---------|
| nvs | data | nvs | 0x9000 | 20KB | Configuration storage |
| phy_init | data | phy | 0xE000 | 4KB | WiFi PHY data |
| factory | app | factory | 0x10000 | 3MB | Application firmware |
| fatfs-react | data | fat | 0x310000 | 960KB | React web interface files |

### Build System

- **Platform**: ESP-IDF (Espressif IoT Development Framework)
- **Build Tool**: PlatformIO
- **Framework**: ESP-IDF (Arduino-free)
- **Board**: esp32dev (configured for ESP32-S3)

### Development Setup

#### Prerequisites

- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- Python 3.7+
- Git

#### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/MladenNikolow/esp32_school_bell.git
   cd esp32_school_bell
   ```

2. Build the firmware:
   ```bash
   pio run
   ```

3. Upload to device:
   ```bash
   pio run --target upload
   ```

4. Monitor serial output:
   ```bash
   pio device monitor
   ```

#### Web Interface Integration

The React web interface from the [esp32_school_bell_web](https://github.com/MladenNikolow/esp32_school_bell_web) repository needs to be built and uploaded to the ESP32's FAT filesystem partition:

1. Build the React application (see esp32_school_bell_web repository for instructions)
2. Copy the built files to the device's FAT partition
3. The ESP32 web server will automatically serve these files

### Configuration

The system supports two WiFi modes:

- **Access Point (AP) Mode**: Creates its own WiFi network for direct connection
- **Station (STA) Mode**: Connects to an existing WiFi network

Configuration can be done through:
- Touch LCD interface
- Web interface (REST API)
- Direct NVS parameter modification

## License

MIT License - Copyright (c) 2026 Mladen Nikolov

See [LICENSE](LICENSE) file for full details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

---

## Development Roadmap

- [x] Core bell control functionality
- [x] WiFi connectivity (AP/STA modes)
- [x] Web server with REST API
- [x] React web interface integration
- [x] NVS configuration storage
- [ ] Touch LCD interface implementation
- [ ] DNS server for AP mode
- [ ] Advanced scheduling features
- [ ] Event logging and history 