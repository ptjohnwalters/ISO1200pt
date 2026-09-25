# MCP2515 CAN Listener for ESP32

This is a standalone PlatformIO test project for checking the MCP2515-to-ESP32 wiring and listening for 250 kbit/s CAN traffic.

## Wiring

| MCP2515 | ESP32 |
|---|---|
| VCC | 5V/VIN, according to the MCP2515 board requirements |
| GND | GND |
| SCK | GPIO 18 |
| SO/MISO | GPIO 19 |
| SI/MOSI | GPIO 23 |
| CS | GPIO 5 |
| INT | GPIO 4 |
| CANH | CAN_H bus |
| CANL | CAN_L bus |

The display, MCP2515, and ESP32 must share ground. The CAN bus should have two 120 ohm terminators, one at each physical end. With power removed, CANH-to-CANL should measure approximately 60 ohms.

## Build, upload, and monitor

From the repository root:

```powershell
cd can_test
python -m platformio run
python -m platformio run -t upload --upload-port COM5
python -m platformio device monitor --port COM5 -b 115200
```

Replace `COM5` if the ESP32 appears on a different port.

Expected startup output:

```text
MCP2515 CAN listener starting...
CAN init OK @250k (8MHz)
Listening...
```

The crystal frequency is detected by trying both 8 MHz and 16 MHz initialization settings. The CAN speed is set to 250 kbit/s.
