#include "plant_control.h"
#include "fold_sequence.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "mcp23017_output.h"

// ─── MCP23017 ADDRESSES ───────────────────────────────────────────────────────
// Same I2C bus as fold_sequence.cpp
// Addresses already initialized in fold_sequence_init()
#define MCP_GPIOA           0x12
#define MCP_GPIOB           0x13

// ─── PLANT MODE SOLENOID STATES ───────────────────────────────────────────────
// Columns = solenoids 1-18 (index 0 = solenoid 1)
// true = 12V energized, false = 0V de-energized

//                                          Sol: 1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18
const bool PLANT_LIMITED_LIFT_STATES[PLANT_SOLENOID_COUNT] = {
    true,  true,  true,  true,  false, false, true,  false, false, false, false, true,  false, false, false, true,  true,  true
};

const bool PLANT_FULL_LIFT_STATES[PLANT_SOLENOID_COUNT] = {
    true,  true,  true,  true,  false, false, true,  false, false, false, false, true,  false, false, false, true,  true,  true
};

const bool PLANT_LOWERED_STATES[PLANT_SOLENOID_COUNT] = {
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

// ─── MODULE STATE VARIABLES ───────────────────────────────────────────────────
static PlantStatus currentStatus = {
    PlantMode::IDLE,    // currentMode
    false,              // sensorSBinEmpty
    false               // isTransitioning
};

// ─── PRIVATE HELPER FUNCTIONS ────────────────────────────────────────────────

// Write a byte to MCP23017 register
static esp_err_t mcp23017_write_plant(uint8_t address, uint8_t reg, uint8_t value)
{
    if ((MCP_GPIOA == reg) || (MCP_GPIOB == reg))
    {
        value = hardware::relay_output_byte(value);
    }
    return mcp23017_write_register_locked(address, reg, value);
}

// Apply solenoid state array to MCP23017 outputs
static esp_err_t write_plant_all_off_locked()
{
    esp_err_t result = ESP_OK;
    const esp_err_t portAResult = mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOA, 0x00);
    const esp_err_t portBResult = mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOB, 0x00);
    const esp_err_t secondPortResult = mcp23017_write_plant(MCP23017_2_ADDR, MCP_GPIOA, 0x00);
    if (ESP_OK != portAResult) result = portAResult;
    if ((ESP_OK == result) && (ESP_OK != portBResult)) result = portBResult;
    if ((ESP_OK == result) && (ESP_OK != secondPortResult)) result = secondPortResult;
    return result;
}

static esp_err_t apply_plant_solenoid_states(const bool states[PLANT_SOLENOID_COUNT])
{
    // Build port A byte for MCP23017 #1 (solenoids 1-8)
    uint8_t mcp1_portA = 0;
    for (int i = 0; i < 8; i++)
    {
        if (states[i]) mcp1_portA |= (1 << i);
    }

    // Build port B byte for MCP23017 #1 (solenoids 9-16)
    uint8_t mcp1_portB = 0;
    for (int i = 0; i < 8; i++)
    {
        if (states[i + 8]) mcp1_portB |= (1 << i);
    }

    // Build port A byte for MCP23017 #2 (solenoids 17-18)
    uint8_t mcp2_portA = 0;
    if (states[16]) mcp2_portA |= (1 << 0);  // Solenoid 17
    if (states[17]) mcp2_portA |= (1 << 1);  // Solenoid 18

    MCP23017OutputLock lock;
    if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;
    esp_err_t result = mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOA, mcp1_portA);
    if (ESP_OK == result) result = mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOB, mcp1_portB);
    if (ESP_OK == result) result = mcp23017_write_plant(MCP23017_2_ADDR, MCP_GPIOA, mcp2_portA);
    if (ESP_OK != result) write_plant_all_off_locked();
    return result;
}

// ─── PUBLIC FUNCTIONS ─────────────────────────────────────────────────────────

esp_err_t plant_control_init()
{
    // I2C already initialized in fold_sequence_init()
    // Just reset state and de-energize all solenoids
    currentStatus.currentMode    = PlantMode::IDLE;
    currentStatus.sensorSBinEmpty = false;
    currentStatus.isTransitioning = false;

    return mcp23017_output_health().has_fault() ? ESP_FAIL : plant_control_all_off();
}

PlantMode plant_control_get_mode()
{
    return currentStatus.currentMode;
}

PlantStatus plant_control_get_status()
{
    return currentStatus;
}

esp_err_t plant_control_set_limited_lift()
{
    currentStatus.isTransitioning = true;
    currentStatus.currentMode = PlantMode::LIMITED_LIFT;
    const esp_err_t result = apply_plant_solenoid_states(PLANT_LIMITED_LIFT_STATES);
    currentStatus.isTransitioning = false;
    return result;
}

esp_err_t plant_control_set_full_lift()
{
    currentStatus.isTransitioning = true;
    currentStatus.currentMode = PlantMode::FULL_LIFT;
    const esp_err_t result = apply_plant_solenoid_states(PLANT_FULL_LIFT_STATES);
    currentStatus.isTransitioning = false;
    return result;
}

esp_err_t plant_control_set_lowered()
{
    currentStatus.isTransitioning = true;
    currentStatus.currentMode = PlantMode::LOWERED;
    const esp_err_t result = apply_plant_solenoid_states(PLANT_LOWERED_STATES);
    currentStatus.isTransitioning = false;
    return result;
}

void plant_control_update_sbin(bool isEmpty)
{
    currentStatus.sensorSBinEmpty = isEmpty;
}

esp_err_t plant_control_apply_current_mode()
{
    switch (currentStatus.currentMode)
    {
        case PlantMode::LIMITED_LIFT:
            return apply_plant_solenoid_states(PLANT_LIMITED_LIFT_STATES);

        case PlantMode::FULL_LIFT:
            return apply_plant_solenoid_states(PLANT_FULL_LIFT_STATES);

        case PlantMode::LOWERED:
        case PlantMode::IDLE:
        default:
            return apply_plant_solenoid_states(PLANT_LOWERED_STATES);
    }
}

esp_err_t plant_control_all_off()
{
    MCP23017OutputLock lock;
    if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;
    return write_plant_all_off_locked();
}

const char* plant_control_get_mode_string()
{
    switch (currentStatus.currentMode)
    {
        case PlantMode::LIMITED_LIFT:   return "LIMITED LIFT";
        case PlantMode::FULL_LIFT:      return "FULL LIFT";
        case PlantMode::LOWERED:        return "LOWERED";
        case PlantMode::IDLE:
        default:                        return "IDLE";
    }
}

const char* plant_control_get_sbin_string()
{
    return currentStatus.sensorSBinEmpty ? "EMPTY" : "FULL";
}