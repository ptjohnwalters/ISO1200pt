#include "fan_vac_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include <algorithm>

// ─── PWM CHANNEL CONFIGURATION ───────────────────────────────────────────────
// ESP32 LEDC peripheral used for PWM generation
// Two independent channels for fan and vac
#define FAN_PWM_CHANNEL     LEDC_CHANNEL_0
#define VAC_PWM_CHANNEL     LEDC_CHANNEL_1
#define PWM_TIMER           LEDC_TIMER_0
#define PWM_MODE            LEDC_HIGH_SPEED_MODE

// ─── ADC CONFIGURATION ───────────────────────────────────────────────────────
#define VAC_ADC_CHANNEL     ADC1_CHANNEL_7   // GPIO35 = ADC1 channel 7
#define ADC_ATTEN           ADC_ATTEN_DB_11  // 0-3.9V input range
#define ADC_WIDTH           ADC_WIDTH_12BIT  // 12 bit resolution 0-4095

// ─── RPM SENSOR VARIABLES ────────────────────────────────────────────────────
static uint32_t fanPulseCount = 0;
static uint32_t lastRPMCalculationTime = 0;
static uint16_t calculatedFanRPM = 0;

// ─── MODULE STATE ────────────────────────────────────────────────────────────
static FanVacStatus currentStatus = {
    FAN_RPM_DEFAULT,        // fanRPMTarget
    0,                      // fanRPMActual
    PWM_MIN_DUTY,           // fanPWMDuty
    false,                  // fanFault
    VAC_PRESSURE_DEFAULT,   // vacPressureTarget
    0,                      // vacPressureActual
    PWM_MIN_DUTY,           // vacPWMDuty
    false,                  // vacFault
    FanVacState::IDLE       // state
};

// ─── PID CONTROLLERS ───────────────────────────────────────────────────────────
static PIDController fanPID = {
    FAN_PID_KP,     // kp
    FAN_PID_KI,     // ki
    FAN_PID_KD,     // kd
    0.0f,           // integral
    0.0f,           // lastError
    PWM_MIN_DUTY,   // outputMin
    PWM_MAX_DUTY    // outputMax
};

static PIDController vacPID = {
    VAC_PID_KP,     // kp
    VAC_PID_KI,     // ki
    VAC_PID_KD,     // kd
    0.0f,           // integral
    0.0f,           // lastError
    PWM_MIN_DUTY,   // outputMin
    PWM_MAX_DUTY    // outputMax
};

// ─── ADC CALIBRATION ───────────────────────────────────────────────────────────
static esp_adc_cal_characteristics_t adcChars;

// ─── PRIVATE HELPER FUNCTIONS ────────────────────────────────────────────────

// Fan RPM pulse counter interrupt handler
static void IRAM_ATTR fan_rpm_isr(void* arg)
{
    fanPulseCount++;
}

// Initialize PWM output channels using ESP32 LEDC peripheral
static void init_pwm_channels()
{
    // Configure PWM timer
    ledc_timer_config_t timerConfig = {};
    timerConfig.speed_mode      = PWM_MODE;
    timerConfig.duty_resolution = LEDC_TIMER_8_BIT;
    timerConfig.timer_num       = PWM_TIMER;
    timerConfig.freq_hz         = PWM_FREQUENCY;
    timerConfig.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&timerConfig);

    // Configure fan PWM channel
    ledc_channel_config_t fanChannel = {};
    fanChannel.gpio_num   = PIN_FAN_PWM;
    fanChannel.speed_mode = PWM_MODE;
    fanChannel.channel    = FAN_PWM_CHANNEL;
    fanChannel.intr_type  = LEDC_INTR_DISABLE;
    fanChannel.timer_sel  = PWM_TIMER;
    fanChannel.duty       = 0;
    fanChannel.hpoint     = 0;
    ledc_channel_config(&fanChannel);

    // Configure vac PWM channel
    ledc_channel_config_t vacChannel = {};
    vacChannel.gpio_num   = PIN_VAC_PWM;
    vacChannel.speed_mode = PWM_MODE;
    vacChannel.channel    = VAC_PWM_CHANNEL;
    vacChannel.intr_type  = LEDC_INTR_DISABLE;
    vacChannel.timer_sel  = PWM_TIMER;
    vacChannel.duty       = 0;
    vacChannel.hpoint     = 0;
    ledc_channel_config(&vacChannel);
}

// Initialize fan RPM sensor interrupt
static void init_rpm_sensor()
{
    gpio_config_t gpioConfig = {};
    gpioConfig.pin_bit_mask = (1ULL << PIN_FAN_RPM_SENSOR);
    gpioConfig.mode         = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type    = GPIO_INTR_POSEDGE;
    gpio_config(&gpioConfig);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(
        (gpio_num_t)PIN_FAN_RPM_SENSOR,
        fan_rpm_isr,
        nullptr
    );
}

// Initialize vac pressure ADC
static void init_vac_adc()
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(VAC_ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adcChars);
}

// Set fan PWM duty cycle
static void set_fan_pwm(uint8_t duty)
{
    ledc_set_duty(PWM_MODE, FAN_PWM_CHANNEL, duty);
    ledc_update_duty(PWM_MODE, FAN_PWM_CHANNEL);
    currentStatus.fanPWMDuty = duty;
}

// Set vac PWM duty cycle
static void set_vac_pwm(uint8_t duty)
{
    ledc_set_duty(PWM_MODE, VAC_PWM_CHANNEL, duty);
    ledc_update_duty(PWM_MODE, VAC_PWM_CHANNEL);
    currentStatus.vacPWMDuty = duty;
}

// ─── PID COMPUTATION ───────────────────────────────────────────────────────────
float pid_compute(PIDController &pid, float setpoint, float measured)
{
    float error = setpoint - measured;

    // Proportional term
    float pTerm = pid.kp * error;

    // Integral term with anti-windup clamping
    pid.integral += error;
    pid.integral = std::max(pid.outputMin, 
                   std::min(pid.outputMax, pid.integral));
    float iTerm = pid.ki * pid.integral;

    // Derivative term
    float dTerm = pid.kd * (error - pid.lastError);
    pid.lastError = error;

    // Compute output and clamp to limits
    float output = pTerm + iTerm + dTerm;
    output = std::max((float)pid.outputMin,
             std::min((float)pid.outputMax, output));

    return output;
}

void pid_reset(PIDController &pid)
{
    pid.integral  = 0.0f;
    pid.lastError = 0.0f;
}

// ─── PUBLIC FUNCTIONS ───────────────────────────────────────────────────────────

void fan_vac_init()
{
    init_pwm_channels();
    init_rpm_sensor();
    init_vac_adc();

    // Start with PWM at zero
    set_fan_pwm(0);
    set_vac_pwm(0);

    // Reset PID controllers
    pid_reset(fanPID);
    pid_reset(vacPID);

    currentStatus.state = FanVacState::IDLE;
}

FanVacStatus fan_vac_get_status()
{
    return currentStatus;
}

FanVacState fan_vac_get_state()
{
    return currentStatus.state;
}

void fan_vac_start()
{
    pid_reset(fanPID);
    pid_reset(vacPID);
    currentStatus.state = FanVacState::RUNNING;
}

void fan_vac_stop()
{
    set_fan_pwm(0);
    set_vac_pwm(0);
    pid_reset(fanPID);
    pid_reset(vacPID);
    currentStatus.state = FanVacState::IDLE;
}

void fan_vac_fan_speed_up()
{
    uint16_t newTarget = currentStatus.fanRPMTarget + FAN_RPM_STEP;
    if (newTarget > FAN_RPM_MAX) newTarget = FAN_RPM_MAX;
    currentStatus.fanRPMTarget = newTarget;
}

void fan_vac_fan_speed_down()
{
    uint16_t newTarget = currentStatus.fanRPMTarget - FAN_RPM_STEP;
    if (newTarget < FAN_RPM_MIN) newTarget = FAN_RPM_MIN;
    currentStatus.fanRPMTarget = newTarget;
}

void fan_vac_vac_pressure_up()
{
    uint16_t newTarget = currentStatus.vacPressureTarget + VAC_PRESSURE_STEP;
    if (newTarget > VAC_PRESSURE_MAX) newTarget = VAC_PRESSURE_MAX;
    currentStatus.vacPressureTarget = newTarget;
}

void fan_vac_vac_pressure_down()
{
    uint16_t newTarget = currentStatus.vacPressureTarget - VAC_PRESSURE_STEP;
    if (newTarget < VAC_PRESSURE_MIN) newTarget = VAC_PRESSURE_MIN;
    currentStatus.vacPressureTarget = newTarget;
}

void fan_vac_set_fan_target(uint16_t targetRPM)
{
    if (targetRPM < FAN_RPM_MIN) targetRPM = FAN_RPM_MIN;
    if (targetRPM > FAN_RPM_MAX) targetRPM = FAN_RPM_MAX;
    currentStatus.fanRPMTarget = targetRPM;
}

void fan_vac_set_vac_target(uint16_t targetPressure)
{
    if (targetPressure < VAC_PRESSURE_MIN) targetPressure = VAC_PRESSURE_MIN;
    if (targetPressure > VAC_PRESSURE_MAX) targetPressure = VAC_PRESSURE_MAX;
    currentStatus.vacPressureTarget = targetPressure;
}

uint16_t fan_vac_read_rpm()
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed = now - lastRPMCalculationTime;

    if (elapsed >= FAN_RPM_SAMPLE_MS)
    {
        // Calculate RPM from pulse count
        // RPM = (pulses / pulses_per_rev) / (elapsed_ms / 60000)
        uint32_t pulses = fanPulseCount;
        fanPulseCount = 0;
        lastRPMCalculationTime = now;

        calculatedFanRPM = (uint16_t)(
            ((float)pulses / FAN_PULSES_PER_REV) *
            (60000.0f / (float)elapsed)
        );

        currentStatus.fanRPMActual = calculatedFanRPM;
    }

    return calculatedFanRPM;
}

uint16_t fan_vac_read_pressure()
{
    // Read raw ADC value
    uint32_t rawADC = adc1_get_raw(VAC_ADC_CHANNEL);

    // Convert to millivolts
    uint32_t millivolts = esp_adc_cal_raw_to_voltage(rawADC, &adcChars);

    // Scale millivolts to pressure units (0mV = min, 3900mV = max)
    uint16_t pressure = (uint16_t)(
        ((float)millivolts / 3900.0f) *
        (VAC_PRESSURE_MAX - VAC_PRESSURE_MIN) +
        VAC_PRESSURE_MIN
    );

    currentStatus.vacPressureActual = pressure;
    return pressure;
}

void fan_vac_update()
{
    if (currentStatus.state != FanVacState::RUNNING)
    {
        return;
    }

    // Read current sensor values
    uint16_t actualRPM      = fan_vac_read_rpm();
    uint16_t actualPressure = fan_vac_read_pressure();

    // Compute fan PID
    float fanOutput = pid_compute(
        fanPID,
        (float)currentStatus.fanRPMTarget,
        (float)actualRPM
    );
    set_fan_pwm((uint8_t)fanOutput);

    // Compute vac PID
    float vacOutput = pid_compute(
        vacPID,
        (float)currentStatus.vacPressureTarget,
        (float)actualPressure
    );
    set_vac_pwm((uint8_t)vacOutput);

    // Check for faults
    // Fan fault if actual RPM is more than 20% below target
    uint16_t fanFaultThreshold = currentStatus.fanRPMTarget * 0.8f;
    currentStatus.fanFault = (actualRPM < fanFaultThreshold);

    // Vac fault if actual pressure is more than 20% below target
    uint16_t vacFaultThreshold = currentStatus.vacPressureTarget * 0.8f;
    currentStatus.vacFault = (actualPressure < vacFaultThreshold);

    // Update overall state if faults detected
    if (currentStatus.fanFault)
    {
        currentStatus.state = FanVacState::FAULT_FAN;
    }
    else if (currentStatus.vacFault)
    {
        currentStatus.state = FanVacState::FAULT_VAC;
    }
    else
    {
        currentStatus.state = FanVacState::RUNNING;
    }
}

const char* fan_vac_get_fan_status_string()
{
    if (currentStatus.fanFault)     return "FAN FAULT";
    if (currentStatus.state == FanVacState::IDLE) return "FAN IDLE";
    return "FAN OK";
}

const char* fan_vac_get_vac_status_string()
{
    if (currentStatus.vacFault)     return "VAC FAULT";
    if (currentStatus.state == FanVacState::IDLE) return "VAC IDLE";
    return "VAC OK";
}
