# ISO1200PT Planter Controller

ISO1200PT is an ESP32-based controller for a Case IH 1200PT planter. It
provides an ISOBUS Virtual Terminal interface for planter controls, calibration,
fold/unfold sequences, fan and vacuum settings, and status information. The
firmware uses an MCP2515 CAN interface, MCP23017 output boards, relay boards,
and the sensors and PWM outputs described in the wiring guide.

> [!CAUTION]
> ## SAFETY
>
> The controller outputs switch hydraulic solenoids. This firmware has **not
> been proven on a machine**. Test the relay boards and solenoids on a bench
> first, without hydraulic pressure. Depressurize the hydraulic system before
> connecting or disconnecting wiring. Never rely on the VT buttons or this
> controller as a safety device; use the machine's independent safety systems.

## Hardware and wiring

See the [WIRING_GUIDE.md](WIRING_GUIDE.md) for pin assignments, CAN and I2C
wiring, MCP23017 addresses, relay polarity, solenoid channel mapping, sensor
connections, and staged bring-up checks. Verify all wiring and output mappings
against the machine schematic before connecting loads.

## Build, flash, and monitor

Install [PlatformIO Core](https://platformio.org/install/cli) and connect the
ESP32 development board over USB. From the repository root, build either
configuration:

```bash
pio run -d examples/1200PT -e esp32dev
pio run -d examples/1200PT -e esp32dev_debug
```

Upload to the connected board (add `--upload-port PORT` if PlatformIO does not
select the port automatically):

```bash
pio run -d examples/1200PT -e esp32dev --target upload
```

Open the serial monitor at 115200 baud:

```bash
pio run -d examples/1200PT -e esp32dev --target monitor
```

## Object pool

`object_pool` is the source XML for the VT object pool. After editing it,
regenerate the embedded binary with:

```bash
python3 generate_iop.py
```

Commit the regenerated `examples/1200PT/src/object_pool/object_pool.iop` with
any pool changes.

## Tests

Run the Python pool and firmware-safety checks:

```bash
python -m unittest discover -s test -p "test_1200pt_*.py"
```

Build and run all standalone C++ unit tests (CMake fetches GoogleTest v1.17.0):

```bash
cmake -S test -B build
cmake --build build
ctest --test-dir build
```

## AgIsoStack++ dependency

PlatformIO uses
[AgIsoStack++](https://github.com/Open-Agriculture/AgIsoStack-plus-plus) at
commit [`5ff90cbe7812ae53e90ecdcf52b016afb52bfa45`](https://github.com/Open-Agriculture/AgIsoStack-plus-plus/commit/5ff90cbe7812ae53e90ecdcf52b016afb52bfa45),
the merge commit of upstream PR #855. The exact commit is pinned so library
updates do not silently change firmware builds. It includes the worker
identification fix required when this firmware initializes its Virtual Terminal
client from ESP-IDF's `app_main`.

The firmware configuration in `examples/1200PT/sdkconfig.defaults` must keep
`CONFIG_FREERTOS_HZ=1000`. Do not lower it: the 1 ms FreeRTOS tick is needed so
short sleeps yield correctly under ESP-IDF 5.0.2.
