#include "fold_sequence.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "mcp23017_output.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

// ─── MCP23017 I2C CONFIGURATION ──────────────────────────────────────────────
// MCP23017 #1 - Solenoids 1-16
// MCP23017 #2 - Solenoids 17-18
// MCP23017 Register Definitions
#define MCP_IODIRA          0x00   // Direction register port A
#define MCP_IODIRB          0x01   // Direction register port B
#define MCP_GPIOA           0x12   // GPIO port A
#define MCP_GPIOB           0x13   // GPIO port B
#define MCP_OLATA           0x14   // Output latch port A
#define MCP_OLATB           0x15   // Output latch port B

// ESP32 I2C Pins
#define I2C_PORT            I2C_NUM_0

// ─── FOLD SEQUENCE SOLENOID STATES ───────────────────────────────────────────
// Rows = steps 1-6
// Columns = solenoids 1-18 (index 0 = solenoid 1)
// true = 12V energized, false = 0V de-energized

//                        Sol: 1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18
const bool FOLD_STATES[FOLD_STEPS][SOLENOID_COUNT] = {
    // Step 1 - Retract Tongue
                             { true,  false, true,  false, false, false, true,  false, false, true,  true,  true,  false, true,  false, true,  true,  true  },
    // Step 2 - Rotate Bar
                             { true,  false, true,  false, true,  false, true,  false, true,  true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 3 - Latch Transport Hooks
                             { true,  false, true,  false, false, false, true,  false, false, false, false, true,  false, false, false, true,  true,  true  },
    // Step 4 - Fold Wings
                             { true,  false, true,  false, true,  false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 5 - Raise Planter
                             { true,  false, true,  false, true,  false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 6 - Latch Wings & Center Bar
                             { true,  false, true,  false, false, false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
};

// ─── UNFOLD SEQUENCE SOLENOID STATES ─────────────────────────────────────────
//                        Sol: 1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18
const bool UNFOLD_STATES[UNFOLD_STEPS][SOLENOID_COUNT] = {
    // Step 1 - Unlatch Wings & Wing Tilt
                             { true,  false, true,  false, true,  true,  true,  true,  true,  false, false, true,  false, false, false, true,  true,  true  },
    // Step 2 - Lower Planter
                             { true,  false, true,  false, true,  false, true,  false, true,  true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 3 - Unfold Wings
                             { true,  false, true,  false, false, false, true,  false, false, true,  true,  true,  false, true,  false, true,  true,  true  },
    // Step 4 - Unlatch Transport Hooks
                             { true,  false, true,  false, false, false, true,  false, false, true,  false, true,  false, true,  false, true,  true,  true  },
    // Step 5 - Rotate Bar
                             { true,  false, true,  false, true,  false, true,  false, false, false, false, true,  false, false, false, true,  true,  true  },
    // Step 6 - Extend Tongue
                             { true,  false, true,  false, true,  false, true,  false, true,  true,  false, true,  false, true,  false, true,  true,  true  },
};

// ─── STEP INSTRUCTION TEXT ───────────────────────────────────────────────────
const char* FOLD_INSTRUCTIONS[FOLD_STEPS] = {
    "1. Retract Tongue",
    "2. Rotate Bar",
    "3. Latch Transport Hooks",
    "4. Fold Wings",
    "5. Raise Planter",
    "6. Latch Wings & Center Bar"
};

const char* UNFOLD_INSTRUCTIONS[UNFOLD_STEPS] = {
    "1. Unlatch Wings & Wing Tilt",
    "2. Lower Planter",
    "3. Unfold Wings",
    "4. Unlatch Transport Hooks",
    "5. Rotate Bar",
    "6. Extend Tongue"
};

// ─── MODULE STATE VARIABLES ───────────────────────────────────────────────────
static SequenceState currentState = SequenceState::IDLE;
static uint8_t currentStep = 0;

// ─── PRIVATE HELPER FUNCTIONS ────────────────────────────────────────────────

// The caller holds MCP23017OutputLock while writing a group of registers.
static esp_err_t mcp23017_write(uint8_t address, uint8_t reg, uint8_t value)
{
    if ((MCP_GPIOA == reg) || (MCP_GPIOB == reg) || (MCP_OLATA == reg) || (MCP_OLATB == reg))
    {
        value = hardware::relay_output_byte(value);
    }
    return mcp23017_write_register_locked(address, reg, value);
}

// Convert solenoid state array to two bytes for MCP23017
// MCP23017 #1 handles solenoids 1-16 (two 8-bit ports)
// MCP23017 #2 handles solenoids 17-18 (first two bits of port A)
static esp_err_t write_all_off_locked()
{
    esp_err_t result = ESP_OK;
    const MCP23017RegisterWrite writes[] = {
        { MCP23017_1_ADDR, MCP_GPIOA, hardware::relay_output_byte(0) },
        { MCP23017_1_ADDR, MCP_GPIOB, hardware::relay_output_byte(0) },
        { MCP23017_2_ADDR, MCP_GPIOA, hardware::relay_output_byte(0) },
        { MCP23017_2_ADDR, MCP_GPIOB, hardware::relay_output_byte(0) }
    };
    for (const auto &write : writes)
    {
        const esp_err_t writeResult = mcp23017_write_register_locked(write.address, write.reg, write.value);
        if ((ESP_OK != writeResult) && (ESP_OK == result)) result = writeResult;
    }
    return result;
}

static esp_err_t apply_solenoid_states(const bool states[SOLENOID_COUNT])
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

    const MCP23017RegisterWrite writes[] = {
        { MCP23017_1_ADDR, MCP_GPIOA, hardware::relay_output_byte(mcp1_portA) },
        { MCP23017_1_ADDR, MCP_GPIOB, hardware::relay_output_byte(mcp1_portB) },
        { MCP23017_2_ADDR, MCP_GPIOA, hardware::relay_output_byte(mcp2_portA) }
    };
    MCP23017OutputLock lock;
    if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;

    esp_err_t result = ESP_OK;
    for (const auto &write : writes)
    {
        result = mcp23017_write_register_locked(write.address, write.reg, write.value);
        if (ESP_OK != result)
        {
            write_all_off_locked();
            break;
        }
    }
    return result;
}

// ─── PUBLIC FUNCTIONS ─────────────────────────────────────────────────────────

esp_err_t fold_sequence_init()
{
    mcp23017_output_init();

    // Initialize I2C bus
    i2c_config_t conf = {};
    conf.mode           = I2C_MODE_MASTER;
    conf.sda_io_num     = MCP23017_I2C_SDA_PIN;
    conf.scl_io_num     = MCP23017_I2C_SCL_PIN;
    conf.sda_pullup_en  = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en  = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = MCP23017_I2C_CLOCK_HZ;
    esp_err_t result = i2c_param_config(I2C_PORT, &conf);
    if (ESP_OK == result)
    {
        result = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
        if (ESP_ERR_INVALID_STATE == result) result = ESP_OK;
    }
    if (ESP_OK != result)
    {
        mcp23017_output_health().record_probe(0, false);
        mcp23017_output_health().record_probe(1, false);
        ESP_LOGE("ISO1200PT", "I2C setup failed; MCP23017 #1 at 0x20 and #2 at 0x21 are MISSING: %s",
                 esp_err_to_name(result));
        return result;
    }

    std::uint8_t registerValue = 0;
    const esp_err_t probeOne = mcp23017_read_register(MCP23017_1_ADDR, MCP_IODIRA, &registerValue);
    const esp_err_t probeTwo = mcp23017_read_register(MCP23017_2_ADDR, MCP_IODIRA, &registerValue);
    mcp23017_output_health().record_probe(0, ESP_OK == probeOne);
    mcp23017_output_health().record_probe(1, ESP_OK == probeTwo);
    ESP_LOGI("ISO1200PT", "MCP23017 #1 at 0x20: %s", ESP_OK == probeOne ? "FOUND" : "MISSING");
    ESP_LOGI("ISO1200PT", "MCP23017 #2 at 0x21: %s", ESP_OK == probeTwo ? "FOUND" : "MISSING");
    if (ESP_OK != probeOne)
    {
        ESP_LOGE("ISO1200PT", "MCP23017 #1 probe failed: %s; hydraulic outputs are locked.",
                 esp_err_to_name(probeOne));
    }
    if (ESP_OK != probeTwo)
    {
        ESP_LOGE("ISO1200PT", "MCP23017 #2 probe failed: %s; hydraulic outputs are locked.",
                 esp_err_to_name(probeTwo));
    }

    // Preload safe logical OFF values into the latches before configuring outputs.
    {
        MCP23017OutputLock lock;
        if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;
        if (ESP_OK == probeOne)
        {
            result = mcp23017_write(MCP23017_1_ADDR, MCP_OLATA, 0x00);
            if (ESP_OK == result) result = mcp23017_write(MCP23017_1_ADDR, MCP_OLATB, 0x00);
        }
        if (ESP_OK == probeTwo)
        {
            const esp_err_t portAResult = mcp23017_write(MCP23017_2_ADDR, MCP_OLATA, 0x00);
            const esp_err_t portBResult = mcp23017_write(MCP23017_2_ADDR, MCP_OLATB, 0x00);
            if (ESP_OK == result) result = portAResult;
            if ((ESP_OK == result) && (ESP_OK != portBResult)) result = portBResult;
        }
    }
    if ((ESP_OK != result) || mcp23017_output_health().has_fault())
    {
        currentState = SequenceState::IDLE;
        currentStep = 0;
        return (ESP_OK == result) ? ESP_FAIL : result;
    }

    // Only configure outputs after both chips responded and their latches are safe.
    {
        MCP23017OutputLock lock;
        if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;
        result = mcp23017_write(MCP23017_1_ADDR, MCP_IODIRA, 0x00);
        if (ESP_OK == result) result = mcp23017_write(MCP23017_1_ADDR, MCP_IODIRB, 0x00);
        if (ESP_OK == result) result = mcp23017_write(MCP23017_2_ADDR, MCP_IODIRA, 0x00);
        if (ESP_OK == result) result = mcp23017_write(MCP23017_2_ADDR, MCP_IODIRB, 0x00);
    }
    if (ESP_OK != result) return result;
    result = fold_sequence_all_off();
    if (ESP_OK != result) return result;

    // Reset state
    currentState = SequenceState::IDLE;
    currentStep = 0;
    return ESP_OK;
}

SequenceState fold_sequence_get_state()
{
    return currentState;
}

uint8_t fold_sequence_get_current_step()
{
    return currentStep + 1;  // Convert to 1-based for display
}

esp_err_t fold_sequence_start_fold()
{
    currentState = SequenceState::FOLD_ACTIVE;
    currentStep = 0;
    const esp_err_t result = fold_sequence_apply_current_step();
    if (ESP_OK != result)
    {
        currentState = SequenceState::IDLE;
        currentStep = 0;
    }
    return result;
}

esp_err_t fold_sequence_start_unfold()
{
    currentState = SequenceState::UNFOLD_ACTIVE;
    currentStep = 0;
    const esp_err_t result = fold_sequence_apply_current_step();
    if (ESP_OK != result)
    {
        currentState = SequenceState::IDLE;
        currentStep = 0;
    }
    return result;
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
        if (ESP_OK != fold_sequence_apply_current_step()) return false;
        return false;  // Sequence still in progress
    }
    else
    {
        // Last step reached - sequence complete
        currentState = SequenceState::COMPLETE;
        if (ESP_OK != fold_sequence_all_off()) return false;
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
        if (ESP_OK != fold_sequence_apply_current_step()) return false;
        return true;  // Successfully went back
    }

    return false;  // Already at first step
}

esp_err_t fold_sequence_cancel()
{
    const esp_err_t result = fold_sequence_all_off();
    currentState = SequenceState::IDLE;
    currentStep = 0;
    return result;
}

esp_err_t fold_sequence_activate_fold_action(uint8_t actionIndex)
{
    if (actionIndex < 1 || actionIndex > FOLD_STEPS)
    {
        return fold_sequence_cancel();
    }
    // De-energize all outputs first to guarantee mutual exclusion
    esp_err_t result = fold_sequence_all_off();
    if (ESP_OK != result) return result;
    currentState = SequenceState::FOLD_ACTIVE;
    currentStep = actionIndex - 1;
    result = apply_solenoid_states(FOLD_STATES[currentStep]);
    if (ESP_OK != result)
    {
        currentState = SequenceState::IDLE;
        currentStep = 0;
    }
    return result;
}

esp_err_t fold_sequence_activate_unfold_action(uint8_t actionIndex)
{
    if (actionIndex < 1 || actionIndex > UNFOLD_STEPS)
    {
        return fold_sequence_cancel();
    }
    // De-energize all outputs first to guarantee mutual exclusion
    esp_err_t result = fold_sequence_all_off();
    if (ESP_OK != result) return result;
    currentState = SequenceState::UNFOLD_ACTIVE;
    currentStep = actionIndex - 1;
    result = apply_solenoid_states(UNFOLD_STATES[currentStep]);
    if (ESP_OK != result)
    {
        currentState = SequenceState::IDLE;
        currentStep = 0;
    }
    return result;
}

uint8_t fold_sequence_get_active_action()
{
    if (currentState == SequenceState::FOLD_ACTIVE || currentState == SequenceState::UNFOLD_ACTIVE)
    {
        return currentStep + 1;
    }
    return 0;
}

esp_err_t fold_sequence_apply_current_step()
{
    if (currentState == SequenceState::FOLD_ACTIVE)
    {
        return apply_solenoid_states(FOLD_STATES[currentStep]);
    }
    else if (currentState == SequenceState::UNFOLD_ACTIVE)
    {
        return apply_solenoid_states(UNFOLD_STATES[currentStep]);
    }
    return ESP_OK;
}

esp_err_t fold_sequence_all_off()
{
    MCP23017OutputLock lock;
    if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;
    return write_all_off_locked();
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