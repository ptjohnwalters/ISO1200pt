# 1200PT ESP32 bench bring-up

This example uses ESP-IDF (not Arduino) and targets an ESP32 Dev Module with
2 MB of flash. The CAN interface uses the ESP32's built-in TWAI peripheral on
GPIO 4 (TX) and GPIO 5 (RX); connect it through a suitable CAN transceiver
before attaching it to an ISOBUS network.

The AgIsoStack++ TWAI compatibility header currently emits an ESP-IDF
deprecation preprocessor warning. The project keeps that warning visible but
does not promote it to an error, so the existing CAN integration remains
usable while the library completes its migration.

## Windows PowerShell

From the repository root:

```powershell
python -m pip install --user platformio
Set-Location examples\1200PT
python -m platformio run -e esp32dev
python -m platformio run -e esp32dev --target upload --upload-port COM3
python -m platformio device monitor --port COM3 --baud 115200
```

Replace `COM3` with the board's serial port. Use `esp32dev_debug` in place of
`esp32dev` to build the debug configuration. If the connected module has more
than 2 MB of flash, leave the configured 2 MB image size unchanged unless its
board definition and `sdkconfig.defaults` are updated together.
