#pragma once

#include <cstdint>

// ─── ESP32 PWM PIN DEFINITIONS ───────────────────────────────────────────────
#define PIN_FAN_PWM          25    // ESP32 GPIO25 - Fan proportional valve PWM
#define PIN_VAC_PWM          26    // ESP32 GPIO26 - Vac proportional valve PWM

// ─── ESP32 SENSOR INPUT PIN DEFINITIONS ──────────────────────────────────────
#define PIN_FAN_RPM_SENSOR   34    // ESP32 GPIO34 - Fan RPM pulse input
#define PIN_VAC_PRESSURE     35    // ESP32 GPIO35 - Vac pressure analog input
#define PIN_SBIN_SENSOR      36    // ESP32 GPIO36 - S bin empty digital input

// ─── PWM CONFIGURATION ───────────────────────────────────────────────────────
// These values need to be confirmed by capturing Pro 600 output
// on your bench before finalizing
#define PWM_FREQUENCY        250   // Hz - typical for proportional valves
                                   // VERIFY with oscilloscope on Pro 600 output
#define PWM_RESOLUTION       8     // 8 bit = 0-255 duty cycle range
#define PWM_MIN_DUTY         51    // 20% minimum duty cycle
#define PWM_MAX_DUTY         204   // 80% maximum duty cycle

// ─── FAN RPM LIMITS ───────────────────────────────────────────────────────────
#define FAN_RPM_MIN          500   // Minimum target RPM
#define FAN_RPM_MAX          3000  // Maximum target RPM
#define FAN_RPM_STEP         50    // RPM increment per button press
#define FAN_RPM_DEFAULT      1500  // Default target RPM on startup

// ─── VAC PRESSURE LIMITS ──────────────────────────────────────────────────────
// Units in tenths of inHg (e.g. 45 = 4.5 inHg)
#define VAC_PRESSURE_MIN     10    // Minimum target pressure
#define VAC_PRESSURE_MAX     100   // Maximum target pressure
#define VAC_PRESSURE_STEP    5     // Pressure increment per button press
#define VAC_PRESSURE_DEFAULT 45    // Default target pressure on startup

// ─── PID TUNING CONSTANTS ─────────────────────────────────────────────────────
// These will need tuning during real world testing at the planter
// Start conservative and adjust based on response

// Fan RPM PID
#define FAN_PID_KP           0.5f  // Proportional gain
#define FAN_PID_KI           0.1f  // Integral gain
#define FAN_PID_KD           0.05f // Derivative gain

// Vac Pressure PID
#define VAC_PID_KP           0.8f  // Proportional gain
#define VAC_PID_KI           0.2f  // Integral gain
#define VAC_PID_KD           0.05f // Derivative gain

// PID update interval in milliseconds
#define PID_UPDATE_INTERVAL  100   // Update PID every 100ms

// ─── FAN RPM SENSOR CONFIGURATION ─────────────────────────────────────────────
// Fan RPM calculated from pulse frequency
// Adjust pulses per revolution to match your specific sensor
#define FAN_PULSES_PER_REV   1     // Pulses per revolution from sensor
#define FAN_RPM_SAMPLE_MS    500   // Sample window for RPM calculation

// ─── VAC PRESSURE SENSOR CONFIGURATION ────────────────────────────────────────
// Vac sensor outputs 0-5V analog signal
// ESP32 ADC reads 0-4095 (12 bit)
// These calibration values convert raw ADC to pressure units
#define VAC_ADC_MIN          0     // ADC reading at minimum pressure
#define VAC_ADC_MAX          4095  // ADC reading at maximum pressure
#define VAC_SCALE_FACTOR     100   // Scaling factor for pressure conversion

// ─── CONTROL STATUS ───────────────────────────────────────────────────────────
enum class FanVacState
{
    IDLE,       // Fan and vac not active
    RUNNING,    // Fan and vac running normally
    FAULT_FAN,  // Fan RPM not reaching target
    FAULT_VAC   // Vac pressure not reaching target
};

// ─── STATUS STRUCT ────────────────────────────────────────────────────────────
struct FanVacStatus
{
    // Fan status
    uint16_t    fanRPMTarget;       // Operator set target RPM
    uint16_t    fanRPMActual;       // Current measured RPM
    uint8_t     fanPWMDuty;         // Current PWM duty cycle 0-255
    bool        fanFault;           // true = fan not reaching target

    // Vac status
    uint16_t    vacPressureTarget;  // Operator set target pressure
    uint16_t    vacPressureActual;  // Current measured pressure
    uint8_t     vacPWMDuty;         // Current PWM duty cycle 0-255
    bool        vacFault;           // true = vac not reaching target

    // Overall state
    FanVacState state;              // Current system state
};

// ─── PID CONTROLLER STRUCT ────────────────────────────────────────────────────
struct PIDController
{
    float kp;           // Proportional gain
    float ki;           // Integral gain
    float kd;           // Derivative gain
    float integral;     // Accumulated integral term
    float lastError;    // Previous error for derivative
    float outputMin;    // Minimum output clamp
    float outputMax;    // Maximum output clamp
};

// ─── FUNCTION DECLARATIONS ────────────────────────────────────────────────────

// Initialize fan and vac control module
// Sets up PWM channels, sensor inputs, PID controllers
void fan_vac_init();

// Get current fan vac status
FanVacStatus fan_vac_get_status();

// Get current system state
FanVacState fan_vac_get_state();

// Start fan and vac control
void fan_vac_start();

// Stop fan and vac - sets PWM to zero
void fan_vac_stop();

// Increase fan RPM target by FAN_RPM_STEP
void fan_vac_fan_speed_up();

// Decrease fan RPM target by FAN_RPM_STEP
void fan_vac_fan_speed_down();

// Increase vac pressure target by VAC_PRESSURE_STEP
void fan_vac_vac_pressure_up();

// Decrease vac pressure target by VAC_PRESSURE_STEP
void fan_vac_vac_pressure_down();

// Set fan RPM target directly
void fan_vac_set_fan_target(uint16_t targetRPM);

// Set vac pressure target directly
void fan_vac_set_vac_target(uint16_t targetPressure);

// Read fan RPM from sensor
// Called from interrupt or timer
uint16_t fan_vac_read_rpm();

// Read vac pressure from analog sensor
uint16_t fan_vac_read_pressure();

// Main update loop - runs PID and updates PWM outputs
// Called every PID_UPDATE_INTERVAL milliseconds
void fan_vac_update();

// Calculate PID output
float pid_compute(PIDController &pid, float setpoint, float measured);

// Reset PID controller state
void pid_reset(PIDController &pid);
