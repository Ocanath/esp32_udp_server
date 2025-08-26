# ESP32 UDP Server

A PlatformIO project for an ESP32-based UDP server with relay control, radar integration, and OTA update capabilities.

## Features

- **UDP Server**: Network communication over UDP protocol
- **Relay Control**: Control relay outputs via UDP commands
- **WiFi Configuration**: Configurable WiFi credentials via serial console
- **OTA Updates**: Over-the-air firmware updates
- **Radar Integration**: Serial communication with radar sensors
- **Console Interface**: Serial command interface for configuration
- **NVS Storage**: Persistent storage of settings in non-volatile storage

## Hardware Requirements

- ESP32 development board
- Relay module (connected to GPIO 25)
- Switch input (connected to GPIO 26)
- LED indicator (connected to GPIO 2)
- Serial radar sensor (connected to Serial2)

## Pin Configuration

- **GPIO 2**: Status LED
- **GPIO 25**: Relay control output
- **GPIO 26**: Switch input
- **Serial2**: Radar sensor communication

## UDP Commands

### Basic Commands
- `marco` - Device discovery (responds with "polo" + MAC address)
- `whoareyou` - Get device name
- `lightson` - Turn relay on
- `lightsoff` - Turn relay off
- `lightstat` - Get relay status

### Configuration Commands (require device name)
- `[name] target-name [new_name]` - Set target device name
- `[name] target-ip [ip_address]` - Set target IP address
- `[name] setignore` - Ignore broadcast commands
- `[name] clearignore` - Allow broadcast commands
- `[name] piperadar` - Enable radar data forwarding
- `[name] stopradar` - Disable radar data forwarding

### Radar Commands
- `activate_hose` - Enable radar data streaming
- `deactivate_hose` - Disable radar data streaming

## Serial Console Commands

### Network Configuration
- `setssid [ssid]` - Set WiFi SSID
- `setpwd [password]` - Set WiFi password
- `setport [port]` - Set UDP server port
- `setTXoff [offset]` - Set reply port offset
- `readcred` - Display stored credentials
- `ipconfig` - Show network configuration
- `reconnect` - Restart WiFi connection

### Device Configuration
- `setname [name]` - Set device name
- `setbaud [baudrate]` - Set Serial2 baud rate
- `setrsize [size]` - Set expected packet size
- `readbaud` - Show current baud rate
- `readrsize` - Show current packet size

### Radar Control
- `plotradar` - Enable radar data display
- `stopradar` - Disable radar data display
- `target-on` - Turn on target device
- `target-off` - Turn off target device

### System Commands
- `restart` - Restart ESP32

## Building and Uploading

### Prerequisites
- PlatformIO IDE or PlatformIO Core
- ESP32 development board
- USB cable for programming

### Build Commands
```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor

# Clean build files
pio run --target clean
```

### PlatformIO Configuration

The project uses the following configuration in `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 460800
upload_speed = 921600
board_build.partitions = default.csv
board_build.flash_mode = qio
board_build.flash_size = 4MB

build_flags = 
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1

lib_deps = 
    arduino-libraries/Arduino_ESP32_OTA@^1.0.0

monitor_filters = esp32_exception_decoder
```

## Project Structure

```
├── platformio.ini          # PlatformIO configuration
├── src/
│   └── main.cpp            # Main application code
├── include/                # Header files
│   ├── nvs.h              # NVS storage definitions
│   ├── parse_console.h    # Console parsing definitions
│   ├── checksum.h         # Checksum calculation definitions
│   ├── circ_scan.h        # Circular buffer definitions
│   └── PPP.h              # PPP protocol definitions
├── lib/                    # Custom libraries
│   ├── nvs/               # NVS storage library
│   ├── parse_console/     # Console parsing library
│   ├── checksum/          # Checksum calculation library
│   ├── circ_scan/         # Circular buffer library
│   └── PPP/               # PPP protocol library
└── README.md              # This file
```

## Libraries

### NVS (Non-Volatile Storage)
Manages persistent storage of WiFi credentials and device settings.

### Parse Console
Handles serial console command parsing and execution.

### Checksum
Provides Fletcher's checksum calculation functions for data integrity.

### Circular Scanner
Implements circular buffer functionality with checksum validation.

### PPP Protocol
Provides PPP byte stuffing and unstuffing for data framing.

## Default Settings

- **Device Name**: relayboard-0000
- **Serial Baud Rate**: 460800
- **UDP Port**: 0 (must be configured)
- **Reply Offset**: 0
- **Expected Words**: 1

## Troubleshooting

### Common Issues

1. **WiFi Connection Fails**
   - Check SSID and password using `readcred`
   - Use `setssid` and `setpwd` to configure
   - Use `reconnect` to restart connection

2. **UDP Commands Not Working**
   - Verify UDP port is set using `udpconfig`
   - Check device name using `whoareyou`
   - Ensure target device is reachable

3. **Serial Communication Issues**
   - Check baud rate using `readbaud`
   - Verify Serial2 connections
   - Use `setbaud` to configure if needed

### Debug Information

The project includes debug output that can be monitored via serial console. Use `pio device monitor` to view debug messages and command responses.

## License

This project is provided as-is for educational and development purposes.