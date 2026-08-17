#include "plant_control.h"
#include "fold_sequence.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

// ─── MCP23017 ADDRESSES ───────────────────────────────────────────────────────
// Same I2C bus as fold_sequence.cpp
// Addresses already initialized in fold_sequence_init()
#define MCP23017_1_ADDR     0x20
#define MCP23017_2_ADDR     0x21
#define MCP_GPIOA           0x12
#define MCP_GPIOB           0x13
#define I2C_PORT            I2C_NUM_0

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
static void mcp23017_write_plant(uint8_t address, uint8_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

// Apply solenoid state array to MCP23017 outputs
static void apply_plant_solenoid_states(const bool states[PLANT_SOLENOID_COUNT])
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

    // Write to MCP23017 #1
    mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOA, mcp1_portA);
    mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOB, mcp1_portB);

    // Write to MCP23017 #2
    mcp23017_write_plant(MCP23017_2_ADDR, MCP_GPIOA, mcp2_portA);
}

// ─── PUBLIC FUNCTIONS ─────────────────────────────────────────────────────────

void plant_control_init()
{
    // I2C already initialized in fold_sequence_init()
    // Just reset state and de-energize all solenoids
    currentStatus.currentMode    = PlantMode::IDLE;
    currentStatus.sensorSBinEmpty = false;
    currentStatus.isTransitioning = false;

    plant_control_all_off();
}

PlantMode plant_control_get_mode()
{
    return currentStatus.currentMode;
}

PlantStatus plant_control_get_status()
{
    return currentStatus;
}

void plant_control_set_limited_lift()
{
    currentStatus.isTransitioning = true;
    currentStatus.currentMode = PlantMode::LIMITED_LIFT;
    apply_plant_solenoid_states(PLANT_LIMITED_LIFT_STATES);
    currentStatus.isTransitioning = false;
}

void plant_control_set_full_lift()
{
    currentStatus.isTransitioning = true;
    currentStatus.currentMode = PlantMode::FULL_LIFT;
    apply_plant_solenoid_states(PLANT_FULL_LIFT_STATES);
    currentStatus.isTransitioning = false;
}

void plant_control_set_lowered()
{
    currentStatus.isTransitioning = true;
    currentStatus.currentMode = PlantMode::LOWERED;
    apply_plant_solenoid_states(PLANT_LOWERED_STATES);
    currentStatus.isTransitioning = false;
}

void plant_control_update_sbin(bool isEmpty)
{
    currentStatus.sensorSBinEmpty = isEmpty;
}

void plant_control_apply_current_mode()
{
    switch (currentStatus.currentMode)
    {
        case PlantMode::LIMITED_LIFT:
            apply_plant_solenoid_states(PLANT_LIMITED_LIFT_STATES);
            break;

        case PlantMode::FULL_LIFT:
            apply_plant_solenoid_states(PLANT_FULL_LIFT_STATES);
            break;

        case PlantMode::LOWERED:
        case PlantMode::IDLE:
        default:
            apply_plant_solenoid_states(PLANT_LOWERED_STATES);
            break;
    }
}

void plant_control_all_off()
{
    mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOA, 0x00);
    mcp23017_write_plant(MCP23017_1_ADDR, MCP_GPIOB, 0x00);
    mcp23017_write_plant(MCP23017_2_ADDR, MCP_GPIOA, 0x00);
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