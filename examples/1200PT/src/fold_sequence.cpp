#include "fold_sequence.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

// ─── MCP23017 I2C CONFIGURATION ──────────────────────────────────────────────
// MCP23017 #1 - Solenoids 1-16
// MCP23017 #2 - Solenoids 17-18
#define MCP23017_1_ADDR     0x20   // I2C address MCP23017 #1 (A0,A1,A2 = GND)
#define MCP23017_2_ADDR     0x21   // I2C address MCP23017 #2 (A0 = VCC, A1,A2 = GND)

// MCP23017 Register Definitions
#define MCP_IODIRA          0x00   // Direction register port A
#define MCP_IODIRB          0x01   // Direction register port B
#define MCP_GPIOA           0x12   // GPIO port A
#define MCP_GPIOB           0x13   // GPIO port B

// ESP32 I2C Pins
#define I2C_SDA_PIN         21     // ESP32 GPIO21 - I2C Data
#define I2C_SCL_PIN         22     // ESP32 GPIO22 - I2C Clock
#define I2C_PORT            I2C_NUM_0
#define I2C_CLK_SPEED_HZ    100000

// ─── FOLD SEQUENCE SOLENOID STATES ───────────────────────────────────────────
// Rows = steps 1-5
// Columns = solenoids 1-18 (index 0 = solenoid 1)
// true = 12V energized, false = 0V de-energized

//                        Sol: 1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18
const bool FOLD_STATES[FOLD_STEPS][SOLENOID_COUNT] = {
    // Step 1 - Inner Marker BACK
                             { true,  false, true,  false, false, false, true,  false, false, false, false, true,  false, false, false, true,  true,  true  },
    // Step 2 - Extend Tongue BACK
                             { true,  false, true,  false, true,  false, true,  false, true,  true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 3 - Set Tongue Lock FLOAT
                             { true,  false, true,  false, false, false, true,  false, false, true,  true,  true,  false, true,  false, true,  true,  true  },
    // Step 4 - Rotate Bar FORWARD
                             { true,  false, true,  false, true,  false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 5 - Set Rotate Lock FORWARD
                             { true,  false, true,  false, true,  false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
};

// ─── UNFOLD SEQUENCE SOLENOID STATES ─────────────────────────────────────────
//                        Sol: 1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18
const bool UNFOLD_STATES[UNFOLD_STEPS][SOLENOID_COUNT] = {
    // Step 1 - Rotate Bar BACK
                             { true,  false, true,  false, true,  true,  true,  true,  true,  false, false, true,  false, false, false, true,  true,  true  },
    // Step 2 - Set Rotate Lock BACK
                             { true,  false, true,  false, true,  false, true,  false, true,  true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 3 - Retract Tongue FORWARD
                             { true,  false, true,  false, false, false, true,  false, false, true,  true,  true,  false, true,  false, true,  true,  true  },
    // Step 4 - Set Tongue Lock FORWARD
                             { true,  false, true,  false, false, false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 5 - Inner Marker FORWARD
                             { true,  false, true,  false, true,  false, true,  false, false, false, false, true,  false, false, false, true,  true,  true  },
};

// ─── STEP INSTRUCTION TEXT ───────────────────────────────────────────────────
const char* FOLD_INSTRUCTIONS[FOLD_STEPS] = {
    "STEP 1: Move Inner Marker BACK",
    "STEP 2: Extend Tongue BACK",
    "STEP 3: Set Tongue Lock FLOAT",
    "STEP 4: Rotate Bar FORWARD",
    "STEP 5: Set Rotate Lock FORWARD"
};

const char* UNFOLD_INSTRUCTIONS[UNFOLD_STEPS] = {
    "STEP 1: Rotate Bar BACK",
    "STEP 2: Set Rotate Lock BACK",
    "STEP 3: Retract Tongue FORWARD",
    "STEP 4: Set Tongue Lock FORWARD",
    "STEP 5: Move Inner Marker FORWARD"
};

// ─── MODULE STATE VARIABLES ───────────────────────────────────────────────────
static SequenceState currentState = SequenceState::IDLE;
static uint8_t currentStep = 0;

// ─── PRIVATE HELPER FUNCTIONS ────────────────────────────────────────────────

// Write a byte to MCP23017 register
static void mcp23017_write(uint8_t address, uint8_t reg, uint8_t value)
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

// Convert solenoid state array to two bytes for MCP23017
// MCP23017 #1 handles solenoids 1-16 (two 8-bit ports)
// MCP23017 #2 handles solenoids 17-18 (first two bits of port A)
static void apply_solenoid_states(const bool states[SOLENOID_COUNT])
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
    mcp23017_write(MCP23017_1_ADDR, MCP_GPIOA, mcp1_portA);
    mcp23017_write(MCP23017_1_ADDR, MCP_GPIOB, mcp1_portB);

    // Write to MCP23017 #2
    mcp23017_write(MCP23017_2_ADDR, MCP_GPIOA, mcp2_portA);
}

// ─── PUBLIC FUNCTIONS ─────────────────────────────────────────────────────────

void fold_sequence_init()
{
    // Initialize I2C bus
    i2c_config_t conf = {};
    conf.mode           = I2C_MODE_MASTER;
    conf.sda_io_num     = I2C_SDA_PIN;
    conf.scl_io_num     = I2C_SCL_PIN;
    conf.sda_pullup_en  = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en  = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_CLK_SPEED_HZ;
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    // Configure MCP23017 #1 - all pins as outputs
    mcp23017_write(MCP23017_1_ADDR, MCP_IODIRA, 0x00);  // Port A all outputs
    mcp23017_write(MCP23017_1_ADDR, MCP_IODIRB, 0x00);  // Port B all outputs

    // Configure MCP23017 #2 - all pins as outputs
    mcp23017_write(MCP23017_2_ADDR, MCP_IODIRA, 0x00);  // Port A all outputs
    mcp23017_write(MCP23017_2_ADDR, MCP_IODIRB, 0x00);  // Port B all outputs

    // De-energize all solenoids on startup
    fold_sequence_all_off();

    // Reset state
    currentState = SequenceState::IDLE;
    currentStep = 0;
}

SequenceState fold_sequence_get_state()
{
    return currentState;
}

uint8_t fold_sequence_get_current_step()
{
    return currentStep + 1;  // Convert to 1-based for display
}

void fold_sequence_start_fold()
{
    currentState = SequenceState::FOLD_ACTIVE;
    currentStep = 0;
    fold_sequence_apply_current_step();
}

void fold_sequence_start_unfold()
{
    currentState = SequenceState::UNFOLD_ACTIVE;
    currentStep = 0;
    fold_sequence_apply_current_step();
}

bool fold_sequence_next_step()
{
    if (currentState == SequenceState::IDLE ||
        currentState == SequenceState::COMPLETE)
    {
        return false;
    }

    uint8_t maxSteps = (currentState == SequenceState::FOLD_ACTIVE)
                       ? FOLD_STEPS
                       : UNFOLD_STEPS;

    if (currentStep < maxSteps - 1)
    {
        currentStep++;
        fold_sequence_apply_current_step();
        return false;  // Sequence still in progress
    }
    else
    {
        // Last step reached - sequence complete
        currentState = SequenceState::COMPLETE;
        fold_sequence_all_off();
        return true;  // Sequence complete
    }
}

bool fold_sequence_prev_step()
{
    if (currentState == SequenceState::IDLE ||
        currentState == SequenceState::COMPLETE)
    {
        return false;
    }

    if (currentStep > 0)
    {
        currentStep--;
        fold_sequence_apply_current_step();
        return true;  // Successfully went back
    }

    return false;  // Already at first step
}

void fold_sequence_cancel()
{
    fold_sequence_all_off();
    currentState = SequenceState::IDLE;
    currentStep = 0;
}

void fold_sequence_apply_current_step()
{
    if (currentState == SequenceState::FOLD_ACTIVE)
    {
        apply_solenoid_states(FOLD_STATES[currentStep]);
    }
    else if (currentState == SequenceState::UNFOLD_ACTIVE)
    {
        apply_solenoid_states(UNFOLD_STATES[currentStep]);
    }
}

void fold_sequence_all_off()
{
    // Write all zeros to both MCP23017s
    mcp23017_write(MCP23017_1_ADDR, MCP_GPIOA, 0x00);
    mcp23017_write(MCP23017_1_ADDR, MCP_GPIOB, 0x00);
    mcp23017_write(MCP23017_2_ADDR, MCP_GPIOA, 0x00);
    mcp23017_write(MCP23017_2_ADDR, MCP_GPIOB, 0x00);
}

const char* fold_sequence_get_instruction()
{
    if (currentState == SequenceState::FOLD_ACTIVE)
    {
        return FOLD_INSTRUCTIONS[currentStep];
    }
    else if (currentState == SequenceState::UNFOLD_ACTIVE)
    {
        return UNFOLD_INSTRUCTIONS[currentStep];
    }
    else if (currentState == SequenceState::COMPLETE)
    {
        return "SEQUENCE COMPLETE";
    }
    return "SELECT FOLD OR UNFOLD";
}