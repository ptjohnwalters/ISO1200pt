#pragma once

#include "isobus/isobus/can_stack_logger.hpp"

// Forward declaration of console logger
extern class ConsoleLogger : public isobus::CANStackLogger
{
public:
    void sink_CAN_stack_log(
        CANStackLogger::LoggingLevel level,
        const std::string &text) override;
} logger;
