# ESP32 VT object-pool example (MCP2515)

This example is configured for an external MCP2515 CAN controller over ESP32 VSPI.

## Wiring (Denky32 / ESP32-WROOM-32)

| MCP2515 | ESP32 |
|---|---|
| VCC | 5V/VIN (per module requirement) |
| GND | GND |
| SCK | GPIO18 |
| SO/MISO | GPIO19 |
| SI/MOSI | GPIO23 |
| CS | GPIO5 |
| INT | GPIO4 |
| CANH | CANH |
| CANL | CANL |

CAN is configured for **250 kbit/s** with an **8 MHz MCP2515 oscillator**.

## Build / upload / monitor

From the repository root:

```bash
cd examples/virtual_terminal/esp32_platformio_object_pool
python -m platformio run -e denky32
python -m platformio run -e denky32 -t upload --upload-port <PORT>
python -m platformio device monitor --port <PORT> -b 115200
```
