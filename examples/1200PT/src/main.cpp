#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/mcp2515_can_interface.hpp"
#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client_update_helper.hpp"
#include "isobus/utility/iop_file_interface.hpp"

#include "console_logger.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

#include "objectPoolObjects.h"
#include "fold_sequence.h"
#include "plant_control.h"
#include "fan_vac_control.h"

#include <functional>
#include <iostream>
#include <memory>

// ─── ISOBUS CLIENT INSTANCES ─────────────────────────────────────────────────
static std::shared_ptr<isobus::VirtualTerminalClient> virtualTerminalClient = nullptr;
static std::shared_ptr<isobus::VirtualTerminalClientUpdateHelper> virtualTerminalUpdateHelper = nullptr;

// ─── ESP32 CAN PIN DEFINITIONS ───────────────────────────────────────────────
#define MCP2515_CS_PIN      GPIO_NUM_15
#define MCP2515_INT_PIN     GPIO_NUM_4
#define MCP2515_SCK_PIN     GPIO_NUM_18
#define MCP2515_MOSI_PIN    GPIO_NUM_23
#define MCP2515_MISO_PIN    GPIO_NUM_19

// ─── TIMERS ──────────────────────────────────────────────────────────────────
static TimerHandle_t pidUpdateTimer     = nullptr;
static TimerHandle_t displayUpdateTimer = nullptr;

// ─── ACTIVE SCREEN TRACKING ──────────────────────────────────────────────────
enum class ActiveScreen
{
    RUN,
    UNFOLD,
    FOLD,
    CAL
};
static ActiveScreen currentScreen = ActiveScreen::RUN;

// ─── FORWARD DECLARATIONS ────────────────────────────────────────────────────
void update_display_values();
void handle_softkey_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);
void handle_button_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);
void navigate_to_screen(ActiveScreen screen);

// ─── NAVIGATE TO SCREEN ──────────────────────────────────────────────────────
void navigate_to_screen(ActiveScreen screen)
{
    uint16_t maskID = DataMask_Run;

    switch (screen)
    {
        case ActiveScreen::RUN:    maskID = DataMask_Run;    break;
        case ActiveScreen::UNFOLD: maskID = DataMask_Unfold; break;
        case ActiveScreen::FOLD:   maskID = DataMask_Fold;   break;
        case ActiveScreen::CAL:    maskID = DataMask_Cal;    break;
    }

    virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
        WorkingSet_1200PT, maskID);
    currentScreen = screen;
}

// ─── PID TIMER CALLBACK ──────────────────────────────────────────────────────
static void pid_timer_callback(TimerHandle_t xTimer)
{
    fan_vac_update();
}

// ─── DISPLAY UPDATE TIMER CALLBACK ───────────────────────────────────────────
static void display_timer_callback(TimerHandle_t xTimer)
{
    update_display_values();
}

// ─── DISPLAY UPDATE FUNCTION ─────────────────────────────────────────────────
void update_display_values()
{
    if (virtualTerminalUpdateHelper == nullptr) return;

    FanVacStatus    fanVacStatus = fan_vac_get_status();
    SequenceStatus  seqStatus    = fold_sequence_get_status();

    // ── RUN Screen values ─────────────────────────────────────────────────────
    // Fan RPM
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_FanRPMActual,
        fanVacStatus.fan.actualValue);
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_FanRPMTarget,
        fanVacStatus.fan.targetValue);

    // Vac pressure
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_VacActual,
        fanVacStatus.vac.actualValue);
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_VacRateTarget,
        fanVacStatus.vac.targetValue);

    // Marker row
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_MarkerRow,
        fanVacStatus.markerRow);

    // ── FOLD/UNFOLD Screen values ─────────────────────────────────────────────
    if (seqStatus.state == SequenceState::FOLD_ACTIVE ||
        seqStatus.state == SequenceState::UNFOLD_ACTIVE)
    {
        virtualTerminalUpdateHelper->set_numeric_value(
            VarNum_FoldStep,
            seqStatus.currentStep + 1);
        virtualTerminalUpdateHelper->set_numeric_value(
            VarNum_UnfoldStep,
            seqStatus.currentStep + 1);
    }
}

// ─── SOFTKEY EVENT HANDLER ───────────────────────────────────────────────────
void handle_softkey_event(
    const isobus::VirtualTerminalClient::VTKeyEvent &event)
{
    if (event.keyEvent !=
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased)
    {
        return;
    }

    switch (event.objectID)
    {
        case SoftKey_Run:
            navigate_to_screen(ActiveScreen::RUN);
            break;

        case SoftKey_Unfold:
            if (fold_sequence_get_state() == SequenceState::FOLD_ACTIVE)
                fold_sequence_cancel();
            navigate_to_screen(ActiveScreen::UNFOLD);
            break;

        case SoftKey_Fold:
            if (fold_sequence_get_state() == SequenceState::UNFOLD_ACTIVE)
                fold_sequence_cancel();
            navigate_to_screen(ActiveScreen::FOLD);
            break;

        case SoftKey_Cal:
            navigate_to_screen(ActiveScreen::CAL);
            break;

        default:
            break;
    }
}

// ─── BUTTON EVENT HANDLER ────────────────────────────────────────────────────
void handle_button_event(
    const isobus::VirtualTerminalClient::VTKeyEvent &event)
{
    if (event.keyEvent !=
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased)
    {
        return;
    }

    switch (event.objectID)
    {
        // ── RUN SCREEN — Vacuum buttons ───────────────────────────────────────
        case Button_VacOn:
            fan_vac_vac_on();
            break;

        case Button_VacOff:
            fan_vac_vac_off();
            break;

        case Button_VacRateUp:
            fan_vac_vac_rate_up();
            break;

        case Button_VacRateDown:
            fan_vac_vac_rate_down();
            break;

        // ── RUN SCREEN — Bulk fill fan buttons ───────────────────────────────
        case Button_FanOn:
            fan_vac_fan_on();
            break;

        case Button_FanOff:
            fan_vac_fan_off();
            break;

        case Button_FanRateUp:
            fan_vac_fan_rate_up();
            break;

        case Button_FanRateDown:
            fan_vac_fan_rate_down();
            break;

        // ── RUN SCREEN — Row marker buttons ──────────────────────────────────
        case Button_MarkerPrev:
            fan_vac_marker_prev();
            break;

        case Button_MarkerNext:
            fan_vac_marker_next();
            break;

        case Button_MarkerAuto:
            break;

        // ── UNFOLD SEQUENCE buttons ───────────────────────────────────────────
        case Button_UnfoldNext:
        {
            if (fold_sequence_get_state() == SequenceState::IDLE ||
                fold_sequence_get_state() == SequenceState::COMPLETE)
            {
                fold_sequence_start_unfold();
            }
            else if (fold_sequence_get_state() == SequenceState::UNFOLD_ACTIVE)
            {
                bool complete = fold_sequence_next_step();
                if (complete)
                    navigate_to_screen(ActiveScreen::RUN);
            }
        }
        break;

        case Button_UnfoldPrev:
            if (fold_sequence_get_state() == SequenceState::UNFOLD_ACTIVE)
                fold_sequence_prev_step();
            break;

        case Button_UnfoldCancel:
            fold_sequence_cancel();
            navigate_to_screen(ActiveScreen::RUN);
            break;

        // ── FOLD SEQUENCE buttons ─────────────────────────────────────────────
        case Button_FoldNext:
        {
            if (fold_sequence_get_state() == SequenceState::IDLE ||
                fold_sequence_get_state() == SequenceState::COMPLETE)
            {
                fold_sequence_start_fold();
            }
            else if (fold_sequence_get_state() == SequenceState::FOLD_ACTIVE)
            {
                bool complete = fold_sequence_next_step();
                if (complete)
                    navigate_to_screen(ActiveScreen::RUN);
            }
        }
        break;

        case Button_FoldPrev:
            if (fold_sequence_get_state() == SequenceState::FOLD_ACTIVE)
                fold_sequence_prev_step();
            break;

        case Button_FoldCancel:
            fold_sequence_cancel();
            navigate_to_screen(ActiveScreen::RUN);
            break;

        // ── CAL SCREEN buttons ────────────────────────────────────────────────
        case Button_CalLimitedBarSet:
            cal_control_set_limited_bar();
            break;

        case Button_CalWingGullSet:
            cal_control_set_wing_gull();
            break;

        case Button_CalHeadlandOn:
            cal_control_headland_on();
            break;

        case Button_CalHeadlandOff:
            cal_control_headland_off();
            break;

        case Button_AlarmAck:
            cal_control_set_error(nullptr);
            navigate_to_screen(ActiveScreen::RUN);
            break;

        default:
            break;
    }
}

// ─── MAIN ENTRY POINT ────────────────────────────────────────────────────────
extern "C" const std::uint8_t object_pool_start[] asm("_binary_object_pool_iop_start");
extern "C" const std::uint8_t object_pool_end[]   asm("_binary_object_pool_iop_end");

extern "C" void app_main()
{
    // ── CAN HARDWARE SETUP ────────────────────────────────────────────────────
    // Initialize MCP2515 CAN interface with proper GPIO configuration
    std::shared_ptr<isobus::CANHardwarePlugin> canDriver =
        std::make_shared<isobus::MCP2515CANInterface>(
            MCP2515_CS_PIN,
            MCP2515_INT_PIN,
            MCP2515_SCK_PIN,
            MCP2515_MOSI_PIN,
            MCP2515_MISO_PIN,
            isobus::MCP2515CANInterface::CANBaudRate::BaudRate250K);

    // ── ISOBUS STACK SETUP ────────────────────────────────────────────────────
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(
        isobus::CANStackLogger::LoggingLevel::Info);
    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, canDriver);

    if (!isobus::CANHardwareInterface::start() || !canDriver->get_is_valid())
    {
        ESP_LOGE("ISO1200PT", "Failed to start CAN hardware interface");
        return;
    }

    // ── ECU IDENTITY ─────────────────────────────────────────────────────────
    isobus::NAME ecuNAME(0);
    ecuNAME.set_arbitrary_address_capable(true);
    ecuNAME.set_industry_group(2);
    ecuNAME.set_device_class(4);
    ecuNAME.set_function_code(25);
    ecuNAME.set_identity_number(1200);
    ecuNAME.set_ecu_instance(0);
    ecuNAME.set_function_instance(0);
    ecuNAME.set_device_class_instance(0);
    ecuNAME.set_manufacturer_code(1407);

    // ── VIRTUAL TERMINAL SETUP ────────────────────────────────────────────────
    const isobus::NAMEFilter filterVT(
        isobus::NAME::NAMEParameters::FunctionCode,
        static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal));
    const std::vector<isobus::NAMEFilter> vtFilters = { filterVT };

    auto internalECU = isobus::CANNetworkManager::CANNetwork
                           .create_internal_control_function(ecuNAME, 0);
    auto partnerVT   = isobus::CANNetworkManager::CANNetwork
                           .create_partnered_control_function(0, vtFilters);

    virtualTerminalClient = std::make_shared<isobus::VirtualTerminalClient>(
        partnerVT, internalECU);
    virtualTerminalClient->set_object_pool(
        0,
        object_pool_start,
        (object_pool_end - object_pool_start),
        "1200");
    virtualTerminalClient->get_vt_soft_key_event_dispatcher()
                          .add_listener(handle_softkey_event);
    virtualTerminalClient->get_vt_button_event_dispatcher()
                          .add_listener(handle_button_event);
    virtualTerminalClient->initialize(true);

    // ── DISPLAY HELPER SETUP ──────────────────────────────────────────────────
    virtualTerminalUpdateHelper =
        std::make_shared<isobus::VirtualTerminalClientUpdateHelper>(
            virtualTerminalClient);

    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_FanRPMActual,  0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_FanRPMTarget,  FAN_RPM_DEFAULT);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_VacActual,     0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_VacRateTarget, VAC_PRESSURE_DEFAULT);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_FoldStep,      1);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_UnfoldStep,    1);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(
        VarNum_MarkerRow,     MARKER_ROW_DEFAULT);
    virtualTerminalUpdateHelper->initialize();

    // ── MODULE INITIALIZATION ─────────────────────────────────────────────────
    fold_sequence_init();
    cal_control_init();
    fan_vac_init();

    // ── TIMERS ────────────────────────────────────────────────────────────────
    pidUpdateTimer = xTimerCreate(
        "PIDTimer",
        pdMS_TO_TICKS(PID_UPDATE_INTERVAL),
        pdTRUE,
        nullptr,
        pid_timer_callback);
    if (pidUpdateTimer != nullptr)
        xTimerStart(pidUpdateTimer, 0);

    displayUpdateTimer = xTimerCreate(
        "DisplayTimer",
        pdMS_TO_TICKS(250),
        pdTRUE,
        nullptr,
        display_timer_callback);
    if (displayUpdateTimer != nullptr)
        xTimerStart(displayUpdateTimer, 0);

    // ── MAIN LOOP ─────────────────────────────────────────────────────────────
    while (true)
    {
        // Poll S bin sensor
        int sBinRaw = gpio_get_level((gpio_num_t)PIN_SBIN_SENSOR);
        bool sBinEmpty = (sBinRaw == 0);
        fan_vac_update_sbin(sBinEmpty);

        // Check for faults
        FanVacStatus fanVacStatus = fan_vac_get_status();
        if (fanVacStatus.fan.isFault &&
            currentScreen != ActiveScreen::CAL)
        {
            cal_control_set_error("BULK FILL FAN FAULT");
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT, DataMask_Alarm);
        }
        else if (fanVacStatus.vac.isFault &&
                 currentScreen != ActiveScreen::CAL)
        {
            cal_control_set_error("VACUUM FAULT");
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT, DataMask_Alarm);
        }

        if (fanVacStatus.sBinEmpty)
            cal_control_set_error("S BIN EMPTY");

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    virtualTerminalClient->terminate();
    isobus::CANHardwareInterface::stop();
}
