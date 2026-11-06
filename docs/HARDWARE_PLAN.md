# 1200PT controller: hardware design and risk review

**Purpose:** pre-purchase/pre-wiring review. This is a source review, not a safety certification, a wiring approval, or evidence that the controller works on a bench or planter. Do not connect pressurized hydraulics until the required items below are resolved and tested.

## Evidence convention and limits

- **[Code]** means verified in the named firmware file/function on repository `main` at `5d9087e26d24b0ddee83d56232d89828e5adf10a`. It describes what source code requests, not measured electrical behavior.
- **[Guide]** means stated in `WIRING_GUIDE.md` and not independently verified against physical hardware.
- **[Assumption]** means a hardware/design inference that must be checked against the actual schematic, component datasheet, or measurement.
- Qualitative risk likelihoods below are engineering estimates, not measured failure rates. This review did not run firmware, a build, hardware tests, or a hydraulic test. PlatformIO was not installed in the review environment, so build status is **unverified**.

## 1. Physical output inventory

MCP23017 output bytes are assembled from the 18-element state arrays in `fold_sequence.cpp` (`apply_solenoid_states`) and `plant_control.cpp` (`apply_plant_solenoid_states`). The pin/tag names and short functional labels below come from `fold_sequence.h` (`SOL_1`…`SOL_18`). The electrical actuator/spool actually connected to each wire is **not** established by firmware. `WIRING_GUIDE.md` §3 describes the relay-channel map; its MCP addresses, board mapping and physical wiring still require continuity/schematic verification.

| Firmware output/tag | MCP23017 pin / ESP32 GPIO | Code label / function | Code command type | Guide comparison |
|---|---|---|---|---|
| SOL_1 | #1 `0x20`, GPA0 | Wing Lift | Logical on/off | Guide maps solenoid 1 to #1 GPA0; agrees. |
| SOL_2 | #1 `0x20`, GPA1 | Raise/Lower | Logical on/off | Guide maps solenoid 2 to #1 GPA1; agrees. |
| SOL_3 | #1 `0x20`, GPA2 | Raise/Lower | Logical on/off | Guide maps solenoid 3 to #1 GPA2; agrees. |
| SOL_4 | #1 `0x20`, GPA3 | Pivot/Latch | Logical on/off | Guide maps solenoid 4 to #1 GPA3; agrees. |
| SOL_5 | #1 `0x20`, GPA4 | Pivot/Latch | Logical on/off | Guide maps solenoid 5 to #1 GPA4; agrees. |
| SOL_6 | #1 `0x20`, GPA5 | Pivot/Latch | Logical on/off | Guide maps solenoid 6 to #1 GPA5; agrees. |
| SOL_7 | #1 `0x20`, GPA6 | Pivot/Latch | Logical on/off | Guide maps solenoid 7 to #1 GPA6; agrees. |
| SOL_8 | #1 `0x20`, GPA7 | Pivot/Latch | Logical on/off | Guide maps solenoid 8 to #1 GPA7; agrees. |
| SOL_9 | #1 `0x20`, GPB0 | Pivot/Latch | Logical on/off | Guide maps solenoid 9 to #1 GPB0; agrees. |
| SOL_10 | #1 `0x20`, GPB1 | Hitch Extend/Latch | Logical on/off | Guide maps solenoid 10 to #1 GPB1; agrees. |
| SOL_11 | #1 `0x20`, GPB2 | Hitch Extend/Latch | Logical on/off | Guide maps solenoid 11 to #1 GPB2; agrees. |
| SOL_12 | #1 `0x20`, GPB3 | Hitch Extend/Latch | Logical on/off | Guide maps solenoid 12 to #1 GPB3; agrees. |
| SOL_13 | #1 `0x20`, GPB4 | Hitch Extend/Latch | Logical on/off | Guide maps solenoid 13 to #1 GPB4; agrees. |
| SOL_14 | #1 `0x20`, GPB5 | Hitch Extend/Latch | Logical on/off | Guide maps solenoid 14 to #1 GPB5; agrees. |
| SOL_15 | #1 `0x20`, GPB6 | Inner/Outer Marker | Logical on/off in fold/plant state arrays | Guide maps solenoid 15 to #1 GPB6; agrees. The separate marker screen controls UI state only; it has no marker output module (`main.cpp`, marker handlers and UI state). |
| SOL_16 | #1 `0x20`, GPB7 | Inner/Outer Marker | Logical on/off | Guide maps solenoid 16 to #1 GPB7; agrees. |
| SOL_17 | #2 `0x21`, GPA0 | Inner/Outer Marker | Logical on/off | Guide maps solenoid 17 to #2 GPA0; agrees. |
| SOL_18 | #2 `0x21`, GPA1 | Inner/Outer Marker | Logical on/off | Guide maps solenoid 18 to #2 GPA1; agrees. |
| Fan command | ESP32 GPIO25 | Fan proportional-valve command | LEDC PWM, 8-bit duty; configured 250 Hz | `WIRING_GUIDE.md` §4 agrees on pin/function and calls 250 Hz a placeholder. Actual driver/load is unspecified. |
| Vacuum command | ESP32 GPIO26 | Vacuum proportional-valve command | LEDC PWM, 8-bit duty; configured 250 Hz | `WIRING_GUIDE.md` §4 agrees on pin/function and calls 250 Hz a placeholder. Actual driver/load is unspecified. |

`mcp23017_output_safety.h` defaults `RELAY_ACTIVE_LOW` to 0: logical ON is written high and logical OFF low. Whether a particular relay input truly triggers high is a physical property, not verified by code. The guide prescribes HIGH-trigger boards and a no-load polarity check (`WIRING_GUIDE.md` §3). It also describes two 8-channel boards plus one 4-channel board; that hardware is guide-reported, not tested here. **No code-to-guide pin-map disagreement was found in the enumerated MCP output or PWM pins.** The functional names remain unverified against the planter schematic.

## 2. End-to-end operation and interlocks

### Fold/unfold and plant hydraulics

- **VT path:** InCommand VT button release events enter `main.cpp` (`handle_button_event`), dispatch to `handle_fold_action_button` / `handle_unfold_action_button`, then call `fold_sequence_activate_fold_action` / `fold_sequence_activate_unfold_action` in `fold_sequence.cpp`. These apply a fixed boolean array to MCP23017 register writes; the guide maps each output to a relay input and then a nominal 12 V solenoid (`WIRING_GUIDE.md` §3). The specific relay contact wiring, coil and hydraulic movement are assumptions until checked against the machine schematic.
- **Fold/unfold states:** `fold_sequence.cpp` (`FOLD_STATES`, `UNFOLD_STATES`) defines six selectable actions each. Every active state energizes SOL_1, SOL_3, SOL_7, SOL_12, SOL_16, SOL_17 and SOL_18. The full energized sets are:

| VT action | Energized SOL tags (from `fold_sequence.cpp`, state array) |
|---|---|
| Fold 1 — Retract Tongue | 1, 3, 7, 10, 11, 12, 14, 16, 17, 18 |
| Fold 2 — Rotate Bar | 1, 3, 5, 7, 9, 10, 12, 14, 16, 17, 18 |
| Fold 3 — Latch Transport Hooks | 1, 3, 7, 12, 16, 17, 18 |
| Fold 4 — Fold Wings | 1, 3, 5, 7, 10, 12, 14, 16, 17, 18 |
| Fold 5 — Raise Planter | 1, 3, 5, 7, 10, 12, 14, 16, 17, 18 |
| Fold 6 — Latch Wings & Center Bar | 1, 3, 7, 10, 12, 14, 16, 17, 18 |
| Unfold 1 — Unlatch Wings & Wing Tilt | 1, 3, 5, 6, 7, 8, 9, 12, 16, 17, 18 |
| Unfold 2 — Lower Planter | 1, 3, 5, 7, 9, 10, 12, 14, 16, 17, 18 |
| Unfold 3 — Unfold Wings | 1, 3, 7, 10, 11, 12, 14, 16, 17, 18 |
| Unfold 4 — Unlatch Transport Hooks | 1, 3, 7, 10, 12, 14, 16, 17, 18 |
| Unfold 5 — Rotate Bar | 1, 3, 5, 7, 12, 16, 17, 18 |
| Unfold 6 — Extend Tongue | 1, 3, 5, 7, 9, 10, 12, 14, 16, 17, 18 |

- **Plant actions:** `main.cpp` (`handle_button_event`) calls `plant_control_set_limited_lift`, `plant_control_set_full_lift`, or `plant_control_set_lowered`; `plant_control.cpp` writes their arrays. Limited Lift and Full Lift arrays are exactly identical: SOL_1,2,3,4,7,12,16,17,18 on. Lowered writes all 18 off. This is a source fact, not proof the two VT commands are mechanically equivalent.
- **Exclusion/lockout rules:** Fold/unfold button handlers reject actions if the startup hydraulic inhibit is still set or `mcp23017_output_health().has_fault()` is true (`main.cpp`, `hydraulic_actuation_inhibited`, fold/unfold handlers). On successful output-board health, the main loop clears the startup inhibit. A board probe or write failure latches output health as faulted (`mcp23017_output_safety.h`, `OutputBoardHealth`; `mcp23017_output.cpp`, `check_command_result`); runtime recovery is not implemented in that object. Fold and unfold are mutually exclusive in the VT handler, which cancels the previous fold state before activating the other (`main.cpp`, `handle_fold_action_button` / `handle_unfold_action_button`). Plant buttons only check the MCP fault; they do not consult the separate startup inhibit (`main.cpp`, plant-button cases). Fan/vacuum state, sensor faults, marker UI state, and VT connection are not hydraulic interlocks. There is no independent emergency-stop input in the reviewed firmware.
- Screen navigation events cancel fold/unfold (`main.cpp`, `handle_softkey_event` / `handle_button_event`); this depends on receiving the VT event. Pressing the currently lit fold/unfold action calls `fold_sequence_cancel`, which attempts to write all outputs off.

### Fan and vacuum

`main.cpp` (`handle_button_event`) maps the VT Fan/Vac power buttons to `fan_vac_toggle_*` and the target +/- buttons to target changes. `fan_vac_control.cpp` (`fan_vac_init`, `fan_vac_start`, `fan_vac_update`) configures LEDC on GPIO25/26 and computes PWM from fan RPM and vacuum-pressure feedback. The expected hardware path is ESP32 3.3 V PWM pin -> a compatible external power/current-control driver -> a valve/controller/motor input. **The code contains no fan/vacuum load driver or wiring definition.** Do not connect a valve coil or motor directly to the ESP32 pin. The relay bank described in the guide is not used for these PWM outputs.

`fan_vac_control.cpp` starts LEDC channels at zero in `fan_vac_init`, but both power-enabled flags initialize `true` and `app_main` immediately calls `fan_vac_start`; on the first update the enabled controllers calculate duty. With zero fan RPM, the default 3200 RPM target drives the fan PID above its 204/255 (80%) ceiling, so the fan command reaches that maximum. With zero vacuum pressure and the default target, the vacuum PID is clamped to its 51/255 (20%) minimum, not its maximum. Thus the initial zero is not a guaranteed sustained OFF state. Also, a fan/vac fault changes the controller state after updating PWM; subsequent `fan_vac_update` calls return early when the state is not `RUNNING`, leaving the last commanded duty in place (`fan_vac_update`). A fault indication therefore does not itself de-energize the command.

### Startup and fault behavior: does the output go OFF?

| Event | What source currently does | Guaranteed physical OFF? |
|---|---|---|
| Normal firmware startup | `app_main` calls `fold_sequence_init`; it probes the expanders, preloads logical OFF and configures outputs only after probes; then calls `plant_control_init`. PWM is initialized to zero, then fan/vac control is started. | **No overall guarantee.** Hydraulic logical OFF is attempted only if I2C is working and relay inputs/contacts default safely. Fan/vac may be commanded nonzero on the first control update. [Code: `fold_sequence_init`, `app_main`, `fan_vac_init`, `fan_vac_start`, `fan_vac_update`; physical outcome is an assumption.] |
| CAN/ISOBUS loss after running | Main loop does not monitor a CAN-loss condition to stop commands; VT-connected status is used only to resynchronize UI state. | **No.** Existing expander outputs and last PWM can remain active. [Code: `app_main` loop.] |
| VT disconnect | Same: only connection edge is used for button/state resynchronization; no disconnect stop handler. | **No.** Hydraulic relays and fan/vac PWM are not shut down by disconnect. [Code: `app_main` loop.] |
| ESP32 watchdog reset / software reset | No explicit watchdog safe-output handler is present in `examples/1200PT/src` (no `esp_task_wdt` handling). Reset re-enters startup, which attempts to set MCP outputs off. | **Not guaranteed.** If the MCP23017 remains powered while only the ESP32 resets, its output latches/relay input levels may persist until reinitialized. Whether the expander also resets is wiring-dependent. [Code: `app_main`, `fold_sequence_init`; hardware outcome is an assumption.] |
| MCP23017 probe/I2C setup failure at startup | Firmware records a latched output fault and inhibits hydraulic buttons; if setup fails it returns an error to `app_main`, which continues CAN/UI initialization. | **No physical guarantee.** A disconnected/stuck bus cannot be commanded off, and unknown relay input bias can energize a channel. [Code: `fold_sequence_init`, `OutputBoardHealth`; physical outcome is an assumption.] |
| MCP23017 write failure while actuating | Fold/apply and plant/apply make best-effort writes of all-off after a partial failure; fault latches and future hydraulic button actions are blocked. Main loop cancels fold sequence when it sees the fault. | **No guarantee.** The all-off retry can fail for the same bus fault; a relay contact can also stick. [Code: `apply_solenoid_states`, `apply_plant_solenoid_states`, `app_main` loop.] |
| Brownout / supply dip | No custom brownout-output handler is in the reviewed source. | **Not guaranteed.** Which devices reset first and whether relay inputs/expander outputs become safe depends on supply wiring and hardware. [Assumption; source checked: `app_main`, `fold_sequence_init`.] |
| Fan RPM / vacuum sensor fault | Firmware marks status fault below 80% of target, but does not command PWM zero; subsequent updates stop when state is a fault. | **No.** Last PWM duty can remain. [Code: `fan_vac_update`.] |

## 3. Fan/vacuum PWM and required output hardware

**PWM method and frequency:** LEDC (ESP32 hardware PWM), not MCP23017 toggling. `fan_vac_control.cpp` (`init_pwm_channels`, `set_fan_pwm`, `set_vac_pwm`) selects 8-bit resolution and `PWM_FREQUENCY`; `fan_vac_control.h` defines 250 Hz, 20–80% nominal duty bounds. The frequency comment calls this a value to confirm; `WIRING_GUIDE.md` §4 also calls 250 Hz a placeholder. The actual configured rate has not been measured. GPIO25 is fan PWM and GPIO26 vacuum PWM. Fan RPM pulses are read on GPIO34 through a rising-edge ISR and sampled by `fan_vac_read_rpm`; vacuum pressure is read from GPIO35 / ADC1 channel 7 in `fan_vac_read_pressure`. GPIO36 is read as active-low S-bin digital input by `app_main`.

**Can opto-isolated mechanical relay boards driven by MCP23017 pins do fan/vac PWM? No.** Mechanical contacts cannot reproduce a 250 Hz duty waveform; they are not speed-control devices and would rapidly wear/arcing-damage. In this firmware they are not even the assigned PWM pins: PWM is direct from ESP32 GPIO25/26. The GPIOs are logic outputs, not power outputs. Use the actual load's specified interface:

- If the output is a **proportional hydraulic valve coil**, use a valve-compatible current/PWM driver with the valve maker's required current regulation, frequency, polarity, and suppression. A generic MOSFET switch may be inadequate if the valve requires controlled coil current or a dither waveform.
- If it is a **PWM input on a fan/vacuum motor controller**, use an interface that matches that controller's input voltage, frequency, polarity and isolation requirements. Do not assume the ESP32 3.3 V signal is accepted.
- If it is a **brushed DC motor/load**, use a properly rated motor MOSFET/H-bridge or motor controller, not the relay board, with transient suppression and thermal protection.
- The source calls these outputs “proportional valve PWM,” but does not establish whether the attached fan/vacuum equipment is a valve, a controller input, a brushed motor, or a brushless motor. This is an open question, not a verified load classification.

Select a driver only after measuring/obtaining load data. For a discrete DC coil, select a logic-compatible automotive-rated MOSFET driver with voltage transient margin, current limiting/thermal protection, and continuous current above the measured hot coil current plus measured turn-on transient, with margin justified by its datasheet and enclosure temperature. Place a flyback diode (or valve-manufacturer-approved TVS/Zener clamp) at each inductive coil; diode cathode goes to the positive supply for a conventional low-side switch. A plain diode slows coil current decay, so confirm valve release time and use an approved clamp if faster release matters. For a motor, size the driver for stall/inrush current, not just running current. **No defensible numeric current rating can be selected from this repository.** The valve/motor manufacturer's PWM frequency and current requirements override the firmware's unvalidated 250 Hz placeholder.

## 4. Are relays/solenoids appropriate?

**Hydraulic fold/unfold/action channels are coded as on/off outputs**, not proportional outputs: the code writes boolean states to MCP23017 register bytes (`fold_sequence.cpp`, `apply_solenoid_states`; `plant_control.cpp`, `apply_plant_solenoid_states`). If the actual valve coils are simple 12 V on/off solenoids, relay switching is electrically plausible at a suitable DC inductive rating. The firmware does not identify a proportional hydraulic coil, and the guide describes 12 V solenoids; nevertheless the actual valve specification and spool mapping are not proven. If a coil needs proportional flow/pressure control, do not use this on/off relay mapping for it; use its specified current-controlled valve driver and rewrite the control accordingly.

Coil current must be measured, not guessed: steady-state estimate is **I ≈ V/R** using measured coil voltage and coil resistance at its temperature; actual current depends on coil temperature, voltage and suppression. Measure the coil's cold resistance, energized steady current at the highest expected supply voltage, and turn-on current with an appropriately rated shunt/current probe. Record the manufacturer's rated current and duty cycle. Also measure the maximum simultaneous energized channels from each state array to size the supply, wiring and fuses.

Inductive current arcs at relay contacts when interrupted. Install a suppression element at every coil, selected for that valve's release-time requirements. The guide's reported “10 A / 30 V DC” relay rating is not independently sourced here and a headline resistive rating is not an inductive solenoid rating. Use the relay manufacturer's **DC inductive/solenoid** contact rating; there is no universal safe percentage derating. If that rating cannot be obtained, do not design up to the 10 A label. A provisional 50% ceiling (5 A) is only a conservative screening limit, **not an approval**; actual inductive rating may require more derating, and board traces, terminals, simultaneous-channel current and ambient temperature may be lower limits.

Planters expose electronics to vibration, dust, moisture, temperature swings, wiring abrasion, and alternator/load transients [Assumption: verify operating environment]. Open opto-isolated relay boards are appropriate for controlled bench bring-up, not a field installation unless enclosed, strain-relieved, sealed and qualified for the environment. The firmware reads no coil current/contact feedback and cannot detect an open coil, shorted coil, welded relay, or wrong spool response (`mcp23017_output.cpp`, writes only; no output readback/diagnostics).

**Recommendation by group:**

- **Hydraulic on/off coils:** do not take exposed hobby-style relay boards to the planter. Prefer sealed automotive relays with verified DC inductive ratings, fused coil feeds, individual coil suppression and sealed connectors/enclosure when the measured coil currents justify relays. A protected automotive high-side/low-side MOSFET driver array is also a good fit if selected for measured current and diagnostics, but requires a deliberate fail-off design. Do not choose the final option until the existing relays' exact model and coil loads are known. Either option is conditional on confirming the 18-channel mapping against the planter schematic.
- **Fan/vacuum speed:** replace any proposed relay path with the load-specific PWM/current driver described in §3. The 18-channel relay bank cannot provide this function.
- **All field outputs:** add a hardware enable/power interlock that defaults OFF independently of software, and prove its behavior during reset, loss of logic power, and stuck I2C. A software “all off” write is not an independent safety mechanism.

## 5. Power and wiring proposal

This is a design proposal, not a verified connector pinout. Use the planter's high-current 12 V plug as the likely power source only after checking its polarity, continuous/peak current rating, and fuse arrangement. Do not assume the ISOBUS connector provides an acceptable supply. Verify its exact connector standard and pinout against the tractor/implement manuals before connecting any power or CAN wires.

1. At the high-current plug, install a main fuse as close to the source as practical, sized for wire ampacity and worst-case branch demand. Split into fused branches: (a) actuator/driver supply and (b) controller electronics. Fuse each coil/driver branch or a group sized for its wire and maximum simultaneous energized current.
2. Feed the electronics branch through reverse-polarity protection and an automotive transient-protected buck converter to the ESP32 board's verified input (typically 5 V only if that specific board supports it). Include a suitably selected TVS at the 12 V input and input/output filtering; choose clamping voltage/energy from the actual vehicle electrical environment and downstream ratings, not a generic TVS part number. Keep MCP23017 logic at the documented 3.3 V supply. Power relay coils/driver logic only from supplies their datasheets permit.
3. Route high-current solenoid/motor returns directly to a power-ground star near the plug/battery return. Bring the buck/controller return to that star separately; avoid carrying coil or motor current through ESP32/MCP23017 ground traces. Share a reference only where the chosen non-isolated driver requires it; preserve isolation where the opto board is actually isolated.
4. Use twisted pair for CAN_H/CAN_L, route away from PWM/coil wiring, and use the appropriate CAN transceiver. MCP2515 is a CAN controller; confirm the specific module includes a compatible CAN physical-layer transceiver and ISOBUS electrical protection. Confirm the ISOBUS pinout and whether this ECU is at a bus end. Terminate only at the two physical ends of the CAN segment; do not add a third 120 ohm terminator. With power off and the bus assembled, approximately 60 ohms across CAN_H/CAN_L is a common check for two 120 ohm end terminators, but topology/vehicle terminations must be verified before measuring or changing them.
5. Keep actuator wiring, fuse protection, connectors and enclosure suitable for vibration, abrasion, dust/moisture and temperature. Fit coil suppression at the load, not only at a remote relay board. Add strain relief and label every wire with its verified schematic signal.

`WIRING_GUIDE.md` §1 gives MCP2515 SPI pins and 250 kbit/s / 8 MHz timing; §2 gives 3.3 V MCP23017, I2C pins/clock and addresses. Those are source/configuration claims, not validation of a purchased module, connector pinout, CAN transceiver, terminations, or EMC robustness.

## 6. Failure and safety review (top 10)

Likelihood is a qualitative pre-test estimate [Assumption], not observed field data. Consequence assumes real hydraulic hardware may move unexpectedly.

| # | Failure mode and likelihood | Consequence | Current evidence / mitigation |
|---|---|---|---|
| 1 | Unexpected energization on boot, reset or brownout — **Medium** | Uncommanded hydraulic movement or fan/vac start | Expander outputs are initialized OFF only after firmware starts and I2C responds; fan/vac start enabled and can command up to 80% on first update. Add hardware-default-OFF enable and inhibit power until explicit operator action; test reset/brownout with expander powered separately. [Code: `fold_sequence_init`, `app_main`, `fan_vac_*`; hardware result assumption.] |
| 2 | Active hydraulic state persists on CAN loss or VT disconnect — **Medium** | Sustained movement/pressure while operator control is unavailable | No communication-loss stop path (`app_main`). Add heartbeat/timeout and a hardware output enable that drops on timeout; define safe-off response and test. |
| 3 | Relay contact weld/stuck output or driver short — **Low–Medium** | Coil remains energized despite software OFF; unexpected motion | No contact/current feedback (`mcp23017_output.cpp`). Use rated automotive drivers/relays, per-channel fusing and suppression, independent power cut, and verify de-energization at the coil. |
| 4 | Hydraulic action mapping/state-table error — **Medium** | Wrong spool, simultaneous conflicting valves, unexpected motion | Source repeats 7 outputs in every fold/unfold step; FOLD 4=5, UNFOLD 2=6, and Plant Limited=Full are identical (`fold_sequence.cpp`, state arrays; `plant_control.cpp`, state arrays). Resolve every state against the OEM schematic and test with coils disconnected before hydraulics. |
| 5 | Software interlock bypass or inadequate safety boundary — **Medium** | Plant action despite fold inhibit, or software/UI state mistaken for safety | Plant handlers check board fault but not the startup inhibit; fold/unfold use both (`main.cpp`). No e-stop/position/load feedback is implemented. Use an independent e-stop/interlocked hydraulic supply and unify the inhibit policy before real hydraulics. |
| 6 | MCP23017/I2C lockup or partial write — **Medium** | Some relays stay on; best-effort all-off write cannot reach board | Writes time out and latch a fault, then attempt all-off; no feedback proves de-energization (`mcp23017_output.cpp`, `fold_sequence.cpp`, `plant_control.cpp`). Add independent driver disable, test bus faults during every output state, and do not rely on retry alone. |
| 7 | Fan/vac PWM applied to wrong load or output driver — **Medium** | Relay wear, damaged GPIO/driver, unintended motor/valve command | Firmware emits 250 Hz on GPIO25/26 but hardware interface is unspecified; GPIO is not a power stage (`fan_vac_control.cpp`). Identify the load and select a compatible protected driver before connecting it. |
| 8 | PWM fault freezes a nonzero command; startup ramps to high duty — **Medium** | Fan/valve remains commanded after detected sensor fault or starts unexpectedly | Fault state prevents later updates without zeroing PWM; initial power flags true and PID output is bounded at 80% (`fan_vac_control.cpp`). Change default state to OFF, require explicit start, and force PWM zero/disable on any fault and communication loss. |
| 9 | Input floats/noise/overvoltage — **Medium** | Incorrect fan RPM, vacuum regulation, or S-bin status; possible ADC damage | GPIO34 and GPIO36 configured without internal pulls; guide requires GPIO34 external pull-up, while GPIO36 bias depends on sensor. GPIO35 must stay in ESP32 limits (`fan_vac_control.cpp`, `app_main`; `WIRING_GUIDE.md` §4). Add specified external bias/conditioning, filtering and voltage protection; verify sensor outputs. |
| 10 | Power/transient/wiring or boot-strap interaction — **Low–Medium** | Reboot, damaged electronics, energized output, loss of control | No input protection design is defined in firmware. GPIO5 is used for MCP2515 CS and is an ESP32 strapping pin [hardware-family assumption; verify exact module behavior]. Validate attached pull states, use automotive power protection and separation, and test cranking/transients/brownout on a current-limited bench supply. |

**Must be resolved before connecting pressurized hydraulics:** verify the complete solenoid/state mapping; identify and rate the loads/relays; establish default-OFF electrical behavior independent of ESP32/I2C; define CAN/VT-loss behavior; fix the PWM startup/fault behavior; and conduct no-pressure output tests with the actual driver, fuse, suppression and e-stop architecture. The present code alone cannot establish that outputs go OFF on these failures.

## 7. Bench test and next steps

Do not put hydraulic pressure on the system during the following electrical bring-up. Each step must pass before moving to the next; record measurements, firmware revision, wiring revision and failures.

1. **Schematic and parts audit:** obtain planter OEM hydraulic/electrical schematic, relay/MCP/ESP32 module datasheets, valve/motor data and connector pinout. **Pass:** each of the 18 tags is tied to one verified load and safe state; current and driver ratings are documented. Otherwise stop.
2. **Logic-only startup:** use a current-limited bench supply; no coils or motors. Verify rails, polarity, MCP addresses, reset pull-ups, grounds, CAN module voltage and ESP32 boot. **Pass:** no excess current, reset loop or hot component; both expanders report present; relay inputs remain inactive during power-up, reset and cable removal.
3. **Relay no-load polarity:** use only the correct isolated logic input, no 12 V coil power; test one channel at a time with a meter/indicator and required bias resistors. **Pass:** each channel activates only at the intended logic level and remains OFF on controller reset/unplug. Do not infer this from an LED alone if relay contacts matter.
4. **Output-map exercise, no loads:** use isolated indicators/logic analyzer to record every fold/unfold/plant state. Compare actual channel map to the signed-off schematic. **Pass:** exact expected channels activate, all unwanted channels remain inactive, and fold-to-unfold replacement first de-energizes the prior state. Confirm duplicate arrays are intentional or correct them before next steps.
5. **Fault injection:** while indicators only are attached, remove each MCP, interrupt I2C, induce a write failure if safely possible, disconnect CAN and VT, and reset/brownout the ESP32 while keeping MCP/driver power on. **Pass:** detected faults block subsequent actuation and the independent hardware enable physically drops all loads. The existing software does not guarantee the physical-off criterion; any failure is a stop condition.
6. **PWM waveform:** with drivers and loads disconnected, scope GPIO25/26 on startup, start/stop, target changes, sensor fault and communication loss. **Pass:** measured frequency/duty match the selected load specification and all stop/fault/reset cases go to the documented safe state. Current code's 250 Hz is only a placeholder and current startup/fault logic is expected to fail the safe-state criterion.
7. **Sensor interface:** simulate RPM pulses with known frequency and pressure sensor voltage with a calibrated source; verify scaling, noise, open/short behavior, and GPIO input bias. **Pass:** values match the sensor data sheet within defined tolerances and open/short produces a safe fault.
8. **Single-coil load test:** only after steps 1–7 pass, use one verified coil at a time on a fused current-limited 12 V supply with approved suppression. Measure cold/hot current, switching transient, release time and driver/terminal temperature. **Pass:** within datasheet and fuse/wire limits, no reset, unintended output, contact welding or overheating.
9. **Simultaneous-load thermal test:** energize the maximum combination appearing in each array (on a safe bench fixture) and test enclosure/environment margin. **Pass:** supply, connectors, wiring and drivers remain within documented ratings. Do not infer this from single-channel testing.
10. **Hydraulic validation last:** only with a qualified operator, mechanically restrained machine, verified e-stop, low-risk setup and a written isolation/abort procedure. Confirm one valve at a time and all position/pressure limits before any sequence. **Pass:** actions match approved OEM sequence and safe-stop response; otherwise disconnect and revise.

### Prioritized changes and purchases

| Priority | Item | Status |
|---|---|---|
| 1 | Verify OEM hydraulic schematic and each SOL_1…SOL_18 channel; resolve repeated outputs and identical action/mode arrays. | **Required before planter** |
| 2 | Add independent default-OFF output enable / emergency isolation that drops on MCU reset, brownout, watchdog, CAN/VT timeout and I2C fault; prove it with the expander separately powered. | **Required before planter** |
| 3 | Define loss-of-CAN/VT policy and implement a tested timeout; turn hydraulic outputs and fan/vac commands off (or other explicitly engineered safe state) on loss. | **Required before planter** |
| 4 | Change fan/vac startup to remain OFF pending explicit enable; zero/disable PWM on sensor fault, output fault, stop, reset/reconnect; verify before attaching a driver. | **Required before planter** |
| 5 | Identify fan/vac equipment and purchase the manufacturer-compatible PWM/current/motor driver, with current/voltage/thermal and transient ratings based on measured load. | **Required before planter** |
| 6 | Measure every hydraulic coil current and relay/driver rating; purchase sealed automotive relays **or** protected automotive MOSFET/high-side driver array, plus approved coil clamps, fuses and sealed connectors. Choose only after measurements and datasheet review. | **Required before planter** |
| 7 | Automotive buck converter, reverse-polarity protection, correctly selected TVS/filter, fused branch wiring and star-ground hardware for the actual 12 V system. | **Required before planter** |
| 8 | Verify ISOBUS connector pinout, CAN transceiver/module compatibility and termination topology with equipment manuals. | **Required before planter** |
| 9 | Add coil-current/open-load/short diagnostics and output feedback; correct the fan pulse counter concurrency and calibrate sensor scaling after the load/sensors are known. | **Recommended** |
| 10 | Sealed enclosure, strain relief, vibration-rated harness, labeled service disconnects and spare fused channels. | **Recommended** |
| 11 | Expand bench fixtures/logging and add independent hydraulic position/pressure feedback if the planter design provides it. | **Optional**, unless required by the OEM safety analysis |

## 8. Open questions for the owner

1. What is the exact planter make/model/year and can you provide the OEM hydraulic and electrical schematics, including each solenoid tag, connector and spool function?
2. Are SOL_1…SOL_18 single coils or paired/directional valve coils? What are each coil's rated voltage, cold resistance, nominal/maximum current, duty cycle and approved suppression method?
3. Which fold/unfold output combinations are intentional? Why are SOL_1,3,7,12,16,17,18 on in every action; why are Fold 4/5, Unfold 2/6 and Limited/Full Lift identical?
4. What exact relay board models are installed? Are 10 A / 30 V DC ratings printed or datasheet-backed, and what are the DC inductive/solenoid ratings, contact topology (NO/NC), input trigger polarity/current, and sealed-enclosure rating?
5. What are the exact fan and vacuum equipment types and part numbers: proportional hydraulic valves, PWM controller inputs, brushed DC motors, brushless motors, or something else? What are their signal voltage/polarity/frequency/duty/current specifications and power-feed requirements?
6. What fan RPM sensor is installed (pulses/revolution, output type, voltage, pull-up requirement)? What vacuum pressure sensor range/output impedance/voltage and calibration are required? What S-bin sensor output type and safe bias state?
7. What is the exact ESP32 board, MCP23017 board, MCP2515 module and relay board part number/revision? Do the CAN modules include a transceiver and protection? What are their supply and logic voltage requirements and reset behavior?
8. What are the high-current 12 V plug polarity, fuse/current rating, wire gauge, source transient/cranking range, and maximum simultaneous coil/motor current?
9. What is the exact ISOBUS connector standard/pinout on both tractor and planter? Which connector supplies CAN, how many terminators are present, where is this ECU located on the bus, and is there another implement ECU sharing it?
10. What is the required safe behavior on loss of CAN, VT, controller power, output-board power, sensor signal, hydraulic pressure, and operator presence? Is there an existing machine e-stop or hydraulic isolation valve, and how is it wired independently of this controller?
11. Should fan/vacuum remain on after VT disconnect or CAN loss? Should they require explicit operator re-enable after every reset/fault? What safe minimum/maximum duty is specified by the equipment maker?
12. Does the system have an output watchdog/current sensor/position feedback already in the harness, or must those be added? What tests and safety standard/OEM procedure govern commissioning?

Until these are answered and the required tests pass, this repository is insufficient evidence to buy the fan/vacuum driver, approve the relay boards for field use, or connect real hydraulic pressure.