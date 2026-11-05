#include "mcp23017_output.h"

#include "esp_log.h"

#include <atomic>

namespace
{
constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
constexpr TickType_t I2C_TIMEOUT = pdMS_TO_TICKS(100);
constexpr const char *TAG = "ISO1200PT";
SemaphoreHandle_t outputBusMutex = nullptr;
std::atomic<bool> writeErrorLogged{false};

esp_err_t check_command_result(esp_err_t result, std::uint8_t address, std::uint8_t reg)
{
    if (ESP_OK != result)
    {
        mcp23017_output_health().record_write_failure();
        bool expected = false;
        if (writeErrorLogged.compare_exchange_strong(expected, true))
        {
            ESP_LOGE(TAG, "MCP23017 write failed at address 0x%02X, register 0x%02X: %s. Hydraulic outputs are locked.",
                     address, reg, esp_err_to_name(result));
        }
    }
    return result;
}
} // namespace

hardware::OutputBoardHealth &mcp23017_output_health()
{
    static hardware::OutputBoardHealth health;
    return health;
}

void mcp23017_output_init()
{
    if (nullptr == outputBusMutex)
    {
        outputBusMutex = xSemaphoreCreateMutex();
        if (nullptr == outputBusMutex)
        {
            mcp23017_output_health().record_write_failure();
            ESP_LOGE(TAG, "Could not create the MCP23017 bus lock. Hydraulic outputs are locked.");
        }
    }
}

MCP23017OutputLock::MCP23017OutputLock()
{
    locked = (nullptr != outputBusMutex) && (pdTRUE == xSemaphoreTake(outputBusMutex, portMAX_DELAY));
    if (!locked)
    {
        mcp23017_output_health().record_write_failure();
        ESP_LOGE(TAG, "Could not lock the MCP23017 I2C bus. Hydraulic outputs are locked.");
    }
}

MCP23017OutputLock::~MCP23017OutputLock()
{
    if (locked) xSemaphoreGive(outputBusMutex);
}

esp_err_t mcp23017_read_register(std::uint8_t address, std::uint8_t reg, std::uint8_t *value)
{
    if ((nullptr == value) || (nullptr == outputBusMutex)) return ESP_ERR_INVALID_STATE;

    MCP23017OutputLock lock;
    if (!lock.is_locked()) return ESP_ERR_INVALID_STATE;

    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (nullptr == command) return ESP_ERR_NO_MEM;
    esp_err_t result = i2c_master_start(command);
    if (ESP_OK == result) result = i2c_master_write_byte(command, (address << 1) | I2C_MASTER_WRITE, true);
    if (ESP_OK == result) result = i2c_master_write_byte(command, reg, true);
    if (ESP_OK == result) result = i2c_master_start(command);
    if (ESP_OK == result) result = i2c_master_write_byte(command, (address << 1) | I2C_MASTER_READ, true);
    if (ESP_OK == result) result = i2c_master_read_byte(command, value, I2C_MASTER_NACK);
    if (ESP_OK == result) result = i2c_master_stop(command);
    if (ESP_OK == result) result = i2c_master_cmd_begin(I2C_PORT, command, I2C_TIMEOUT);
    i2c_cmd_link_delete(command);
    return result;
}

esp_err_t mcp23017_write_register_locked(std::uint8_t address, std::uint8_t reg, std::uint8_t value)
{
    if (nullptr == outputBusMutex)
    {
        return check_command_result(ESP_ERR_INVALID_STATE, address, reg);
    }

    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (nullptr == command) return check_command_result(ESP_ERR_NO_MEM, address, reg);
    esp_err_t result = i2c_master_start(command);
    if (ESP_OK == result) result = i2c_master_write_byte(command, (address << 1) | I2C_MASTER_WRITE, true);
    if (ESP_OK == result) result = i2c_master_write_byte(command, reg, true);
    if (ESP_OK == result) result = i2c_master_write_byte(command, value, true);
    if (ESP_OK == result) result = i2c_master_stop(command);
    if (ESP_OK == result) result = i2c_master_cmd_begin(I2C_PORT, command, I2C_TIMEOUT);
    i2c_cmd_link_delete(command);
    return check_command_result(result, address, reg);
}
