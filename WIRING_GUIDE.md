# 1200PT ESP32 Hardware Wiring Guide

## Hardware Overview
- **Microcontroller:** ESP32-WROOM-32 (38-pin)
- **CAN Interface:** MCP2515 SPI module with TJA1050 transceiver
- **I/O Expansion:** 2x MCP23017 I2C GPIO Expanders (16 channels each = 32 GPIO)
- **Relays:** 4-channel + 8-channel relay modules for solenoid control
- **Level Shifter:** 3.3V ↔ 5V logic level converter
- **Power:** 5V for relays/sensors, 3.3V for ESP32/I2C

---

## Part 1: ESP32 Core Connections

### ESP32 Power
```
ESP32 GND     → Breadboard GND rail
ESP32 3V3     → Breadboard 3.3V rail
ESP32 5V      → (Only if powered via USB; otherwise connect 5V power supply)
```

---

## Part 2: MCP2515 CAN Bus Module → ESP32 (SPI Interface)

**MCP2515 Module Pinout:**
- VCC, GND, CS, MOSI, MISO, SCK, INT

**Connections:**
```
MCP2515 VCC    → 3.3V rail (via level shifter if needed)
MCP2515 GND    → GND rail
MCP2515 CS     → ESP32 GPIO15 (SPI Chip Select)
MCP2515 MOSI   → ESP32 GPIO23 (SPI Master Out)
MCP2515 MISO   → ESP32 GPIO19 (SPI Master In)
MCP2515 SCK    → ESP32 GPIO18 (SPI Clock)
MCP2515 INT    → ESP32 GPIO4 (Interrupt, optional but recommended)
```

**TJA1050 Transceiver on MCP2515 Module:**
```
CANH  → To ISOBUS network (or InCommand display CAN H)
CANL  → To ISOBUS network (or InCommand display CAN L)
GND   → Same GND as ESP32
```

---

## Part 3: I2C Level Shifter → MCP23017s

### Level Shifter Module (3.3V ↔ 5V)
```
LOW side (3.3V):
  GND      → ESP32 GND
  3V3      → ESP32 3.3V
  SDA1     → ESP32 GPIO21 (I2C SDA)
  SCL1     → ESP32 GPIO22 (I2C SCL)

HIGH side (5V):
  GND      → Relay modules GND
  5V       → 5V power supply
  SDA2     → MCP23017 #1 SDA (via resistor to 5V)
  SCL2     → MCP23017 #1 SCL (via resistor to 5V)
```

### MCP23017 #1 (Fold/Unfold Solenoids - Lower 8 bits + Upper 8 bits)
**Address:** A2=0, A1=0, A0=0 → I2C 0x20
```
VCC   → 5V
GND   → GND
SDA   → Level shifter SDA2
SCL   → Level shifter SCL2
A0    → GND
A1    → GND
A2    → GND

Port A (PA0-PA7):  → Relay Module #1 CH1-CH4, Relay Module #2 CH1-CH4
Port B (PB0-PB7):  → (Reserved or future solenoids)
```

### MCP23017 #2 (Additional Solenoids/Sensors)
**Address:** A2=0, A1=0, A0=1 → I2C 0x21
```
VCC   → 5V
GND   → GND
SDA   → Level shifter SDA2
SCL   → Level shifter SCL2
A0    → 5V (pulled high via 10kΩ resistor)
A1    → GND
A2    → GND

Port A (PA0-PA7):  → Relay Module #2 CH5-CH8
Port B (PB0-PB7):  → (Reserved or future use)
```

---

## Part 4: Relay Modules → Solenoids

### 4-Channel Relay Module (Relay #1)
**Power:**
```
VCC   → 5V power supply
GND   → GND
JD-VCC → 5V (connected via jumper if using separate power)
GND   → GND
```

**Control (from MCP23017 #1 Port A):**
```
IN1   → MCP23017 #1 PA0 (via level shifter if needed)
IN2   → MCP23017 #1 PA1
IN3   → MCP23017 #1 PA2
IN4   → MCP23017 #1 PA3
```

**Relay Outputs (to Solenoids):**
```
NO1/COM1  → Fold solenoid A
NO2/COM2  → Fold solenoid B
NO3/COM3  → Unfold solenoid A
NO4/COM4  → Unfold solenoid B
```

### 8-Channel Relay Module #1 (Relay #2)
**Power:**
```
VCC   → 5V power supply
GND   → GND
JD-VCC → 5V
GND   → GND
```

**Control (from MCP23017 #1 Port A):**
```
IN1   → MCP23017 #1 PA4
IN2   → MCP23017 #1 PA5
IN3   → MCP23017 #1 PA6
IN4   → MCP23017 #1 PA7
IN5   → (unused)
IN6   → (unused)
IN7   → (unused)
IN8   → (unused)
```

**Relay Outputs:**
```
NO1/COM1  → Plant lift solenoid
NO2/COM2  → Plant lower solenoid
NO3/COM3  → (spare)
NO4/COM4  → (spare)
NO5/COM5  → (spare)
NO6/COM6  → (spare)
NO7/COM7  → (spare)
NO8/COM8  → (spare)
```

### 8-Channel Relay Module #2 (Relay #3)
**Power:**
```
VCC   → 5V power supply
GND   → GND
JD-VCC → 5V
GND   → GND
```

**Control (from MCP23017 #2 Port A):**
```
IN1   → MCP23017 #2 PA0
IN2   → MCP23017 #2 PA1
IN3   → MCP23017 #2 PA2
IN4   → MCP23017 #2 PA3
IN5-IN8 → (unused)
```

**Relay Outputs (Future Use)**

---

## Part 5: PWM Outputs (Fan & Vac Control)

### ESP32 PWM Pins (Direct to Valve Driver)
```
ESP32 GPIO25  → Fan PWM proportional valve driver
                (Expects 0-255 PWM, 250Hz)
                
ESP32 GPIO26  → Vac PWM proportional valve driver
                (Expects 0-255 PWM, 250Hz)
```

**If using external MOSFET drivers:**
```
GPIO25 → MOSFET gate (via 1kΩ resistor)
GPIO26 → MOSFET gate (via 1kΩ resistor)
```

---

## Part 6: Sensor Inputs

### RPM Sensor (Fan)
```
ESP32 GPIO34  → Fan RPM pulse input (pulled high internally)
                Connect fan sensor positive through optocoupler if needed
```

### Vacuum Pressure Sensor (Analog)
```
ESP32 GPIO35  → ADC input for vac pressure sensor (0-3.9V range)
Sensor GND    → GND rail
Sensor VCC    → 5V (with voltage divider if sensor outputs 5V)
```

### S-Bin Empty Sensor (Digital)
```
ESP32 GPIO36  → S-bin empty digital input
Sensor GND    → GND rail
Sensor VCC    → 5V
```

---

## Part 7: Power Supply Layout

```
┌─────────────────────────────────────────┐
│       External 5V Power Supply          │
│         (10A recommended)               │
└─────────────────────────────────────────┘
            ↓
      ┌─────────────┐
      │  Relay GND  │ (for relay coils)
      │  Relay +5V  │
      └─────────────┘
            ↓
┌─────────────────────────────────────────┐
│       Breadboard Power Rails            │
│  GND (common)  |  +5V  |  +3.3V        │
└─────────────────────────────────────────┘
      ↓              ↓         ↓
  All modules    Relays   ESP32/I2C/Level
                          Shifter low side
```

---

## Part 8: I2C Pullup Resistors

Add 10kΩ pullup resistors on I2C lines:
```
ESP32 GPIO21 (SDA) → +3.3V via 10kΩ resistor
ESP32 GPIO22 (SCL) → +3.3V via 10kΩ resistor

MCP23017 SDA → +5V via 10kΩ resistor (on high side)
MCP23017 SCL → +5V via 10kΩ resistor (on high side)
```

---

## Part 9: Summary Table

| Device | Connection | GPIO/Pin | Function |
|--------|-----------|----------|----------|
| MCP2515 CS | ESP32 | GPIO15 | SPI Chip Select |
| MCP2515 MOSI | ESP32 | GPIO23 | SPI Data Out |
| MCP2515 MISO | ESP32 | GPIO19 | SPI Data In |
| MCP2515 SCK | ESP32 | GPIO18 | SPI Clock |
| MCP23017 #1/2 SDA | ESP32 | GPIO21 | I2C Data |
| MCP23017 #1/2 SCL | ESP32 | GPIO22 | I2C Clock |
| Fan RPM Sensor | ESP32 | GPIO34 | RPM Input |
| Vac Pressure | ESP32 | GPIO35 | ADC Input |
| S-Bin Sensor | ESP32 | GPIO36 | Digital Input |
| Fan PWM Valve | ESP32 | GPIO25 | PWM Output |
| Vac PWM Valve | ESP32 | GPIO26 | PWM Output |

---

## Testing Checklist

- [ ] ESP32 powers on (LED blinks)
- [ ] I2C devices detected (MCP23017s)
- [ ] CAN bus communication (check logs)
- [ ] Relays switch on/off (audible click)
- [ ] PWM outputs working (oscilloscope on GPIO25/26)
- [ ] ADC reading vac pressure (0-4095 value changes)
- [ ] RPM sensor counting pulses
- [ ] S-bin sensor detecting empty state

---

## Troubleshooting

**No I2C communication:**
- Check pullup resistors
- Verify level shifter connections
- Check MCP23017 address jumpers

**No CAN communication:**
- Verify MCP2515 SPI connections
- Check crystal frequency (8MHz typical)
- Measure voltages on CANH/CANL

**Relays not switching:**
- Check IN pins connected to MCP23017
- Verify relay power supply
- Check relay module jumper settings (HIGH/LOW trigger)

