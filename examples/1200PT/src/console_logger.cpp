#include "isobus/isobus/can_stack_logger.hpp"
#include "esp_log.h"
#include <string>

// ─── CONSOLE LOGGER ──────────────────────────────────────────────────────────
// Routes AgIsoStack++ log messages to ESP32 console output
// Useful for debugging ISOBUS communication during development
// Tag used for all ESP32 log output
static const char* TAG = "ISO1200PT";

class ConsoleLogger : public isobus::CANStackLogger
{
public:
    void sink_CAN_stack_log(
        CANStackLogger::LoggingLevel level,
        const std::string &text) override
    {
        switch (level)
        {
            case LoggingLevel::Debug:
                ESP_LOGD(TAG, "%s", text.c_str());
                break;

            case LoggingLevel::Info:
                ESP_LOGI(TAG, "%s", text.c_str());
                break;

            case LoggingLevel::Warning:
                ESP_LOGW(TAG, "%s", text.c_str());
                break;

            case LoggingLevel::Critical:
                ESP_LOGE(TAG, "%s", text.c_str());
                break;

            default:
                ESP_LOGI(TAG, "%s", text.c_str());
                break;
        }
    }
};

// Single global instance used by main.cpp
ConsoleLogger logger;