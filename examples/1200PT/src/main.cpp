#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/twai_plugin.hpp"
#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client_update_helper.hpp"
#include "isobus/utility/iop_file_interface.hpp"

#include "console_logger.cpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

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
// MCP2515 connects to ESP32 via SPI
// TWAI (ESP32 built in CAN) used for ISOBUS
#define CAN_TX_PIN      GPIO_NUM_4    // ESP32 GPIO4  - CAN TX to MCP2515
#define CAN_RX_PIN      GPIO_NUM_5    // ESP32 GPIO5  - CAN RX from MCP2515

// ─── PID UPDATE TIMER ────────────────────────────────────────────────────────
static TimerHandle_t pidUpdateTimer = nullptr;

// ─── SCREEN TRACKING ─────────────────────────────────────────────────────────
// Tracks which screen is currently active on InCommand
enum class ActiveScreen
{
    HOME,
    FOLD,
    UNFOLD,
    PLANT,
    FAN_VAC
};
static ActiveScreen currentScreen = ActiveScreen::HOME;

// ─── FORWARD DECLARATIONS ────────────────────────────────────────────────────
void update_display_values();
void handle_softkey_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);
void handle_button_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);

// ─── PID TIMER CALLBACK ──────────────────────────────────────────────────────
// Called every PID_UPDATE_INTERVAL milliseconds
// Updates fan and vac PID loops and refreshes display values
static void pid_timer_callback(TimerHandle_t xTimer)
{
    fan_vac_update();
    update_display_values();
}

// ─── DISPLAY UPDATE FUNCTION ─────────────────────────────────────────────────
// Pushes current sensor values and status to InCommand display
void update_display_values()
{
    if (virtualTerminalUpdateHelper == nullptr) return;

    FanVacStatus fanVacStatus = fan_vac_get_status();
    PlantStatus plantStatus   = plant_control_get_status();

    // Update fan RPM actual display
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_FanRPMActual,
        fanVacStatus.fanRPMActual
    );

    // Update fan RPM target display
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_FanRPMTarget,
        fanVacStatus.fanRPMTarget
    );

    // Update vac pressure actual display
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_VacActual,
        fanVacStatus.vacPressureActual
    );

    // Update vac pressure target display
    virtualTerminalUpdateHelper->set_numeric_value(
        VarNum_VacTarget,
        fanVacStatus.vacPressureTarget
    );

    // Update fold step display if sequence active
    if (fold_sequence_get_state() == SequenceState::FOLD_ACTIVE ||
        fold_sequence_get_state() == SequenceState::UNFOLD_ACTIVE)
    {
        virtualTerminalUpdateHelper->set_numeric_value(
            VarNum_FoldStep,
            fold_sequence_get_current_step()
        );
    }
}

// ─── SOFTKEY EVENT HANDLER ───────────────────────────────────────────────────
// Handles navigation soft keys on InCommand display
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
        case SoftKey_Home:
            // Return to home screen
            // Cancel any active sequence first
            if (fold_sequence_get_state() != SequenceState::IDLE)
            {
                fold_sequence_cancel();
            }
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Home
            );
            currentScreen = ActiveScreen::HOME;
            break;

        case SoftKey_Fold:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Fold
            );
            currentScreen = ActiveScreen::FOLD;
            break;

        case SoftKey_Unfold:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Unfold
            );
            currentScreen = ActiveScreen::UNFOLD;
            break;

        case SoftKey_Plant:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Plant
            );
            currentScreen = ActiveScreen::PLANT;
            break;

        case SoftKey_FanVac:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_FanVac
            );
            currentScreen = ActiveScreen::FAN_VAC;
            break;

        default:
            break;
    }
}

// ─── BUTTON EVENT HANDLER ────────────────────────────────────────────────────
// Handles all button presses on InCommand display
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
        // ── HOME SCREEN NAVIGATION ────────────────────────────────────────────
        case Button_GoToFold:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Fold
            );
            currentScreen = ActiveScreen::FOLD;
            break;

        case Button_GoToUnfold:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Unfold
            );
            currentScreen = ActiveScreen::UNFOLD;
            break;

        case Button_GoToPlant:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Plant
            );
            currentScreen = ActiveScreen::PLANT;
            break;

        case Button_GoToFanVac:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_FanVac
            );
            currentScreen = ActiveScreen::FAN_VAC;
            break;

        // ── FOLD SEQUENCE BUTTONS ─────────────────────────────────────────────
        case Button_FoldNext:
        {
            if (fold_sequence_get_state() == SequenceState::IDLE ||
                fold_sequence_get_state() == SequenceState::COMPLETE)
            {
                // Start fold sequence
                fold_sequence_start_fold();
            }
            else
            {
                // Advance to next step
                bool complete = fold_sequence_next_step();
                if (complete)
                {
                    // Sequence complete - show completion message
                    virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                        WorkingSet_1200PT,
                        DataMask_Home
                    );
                    currentScreen = ActiveScreen::HOME;
                }
            }
        }
        break;

        case Button_FoldPrev:
            fold_sequence_prev_step();
            break;

        case Button_FoldCancel:
            fold_sequence_cancel();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Home
            );
            currentScreen = ActiveScreen::HOME;
            break;

        // ── UNFOLD SEQUENCE BUTTONS ───────────────────────────────────────────
        case Button_UnfoldNext:
        {
            if (fold_sequence_get_state() == SequenceState::IDLE ||
                fold_sequence_get_state() == SequenceState::COMPLETE)
            {
                // Start unfold sequence
                fold_sequence_start_unfold();
            }
            else
            {
                // Advance to next step
                bool complete = fold_sequence_next_step();
                if (complete)
                {
                    virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                        WorkingSet_1200PT,
                        DataMask_Home
                    );
                    currentScreen = ActiveScreen::HOME;
                }
            }
        }
        break;

        case Button_UnfoldPrev:
            fold_sequence_prev_step();
            break;

        case Button_UnfoldCancel:
            fold_sequence_cancel();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Home
            );
            currentScreen = ActiveScreen::HOME;
            break;

        // ── PLANT MODE BUTTONS ────────────────────────────────────────────────
        case Button_PlantLimitedLift:
            plant_control_set_limited_lift();
            break;

        case Button_PlantFullLift:
            plant_control_set_full_lift();
            break;

        case Button_PlantLower:
            plant_control_set_lowered();
            break;

        // ── FAN VAC BUTTONS ───────────────────────────────────────────────────
        case Button_FanSpeedUp:
            fan_vac_fan_speed_up();
            break;

        case Button_FanSpeedDown:
            fan_vac_fan_speed_down();
            break;

        case Button_VacPressureUp:
            fan_vac_vac_pressure_up();
            break;

        case Button_VacPressureDown:
            fan_vac_vac_pressure_down();
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
    twai_general_config_t twaiConfig = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_PIN,
        CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t twaiTiming  = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t twaiFilter  = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    std::shared_ptr<isobus::CANHardwarePlugin> canDriver =
        std::make_shared<isobus::TWAIPlugin>(
            &twaiConfig,
            &twaiTiming,
            &twaiFilter
        );

    // ── ISOBUS STACK SETUP ────────────────────────────────────────────────────
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(
        isobus::CANStackLogger::LoggingLevel::Info
    );
    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, canDriver);

    if (!isobus::CANHardwareInterface::start() || !canDriver->get_is_valid())
    {
        ESP_LOGE("ISO1200PT", "Failed to start CAN hardware interface");
        return;
    }

    // ── ECU IDENTITY SETUP ────────────────────────────────────────────────────
    // Identifies this ECU on the ISOBUS network
    // Function code 25 = Tillage - closest match for planter frame control
    isobus::NAME ecuNAME(0);
    ecuNAME.set_arbitrary_address_capable(true);
    ecuNAME.set_industry_group(2);              // Agriculture
    ecuNAME.set_device_class(4);               // Seeding
    ecuNAME.set_function_code(25);             // Tillage/Planter Frame
    ecuNAME.set_identity_number(1200);         // 1200PT identifier
    ecuNAME.set_ecu_instance(0);
    ecuNAME.set_function_instance(0);
    ecuNAME.set_device_class_instance(0);
    ecuNAME.set_manufacturer_code(1407);       // Keep from example

    // ── VIRTUAL TERMINAL SETUP ────────────────────────────────────────────────
    const isobus::NAMEFilter filterVT(
        isobus::NAME::NAMEParameters::FunctionCode,
        static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal)
    );
    const std::vector<isobus::NAMEFilter> vtFilters = { filterVT };

    auto internalECU = isobus::CANNetworkManager::CANNetwork
                           .create_internal_control_function(ecuNAME, 0);
    auto partnerVT   = isobus::CANNetworkManager::CANNetwork
                           .create_partnered_control_function(0, vtFilters);

    virtualTerminalClient = std::make_shared<isobus::VirtualTerminalClient>(
        partnerVT,
        internalECU
    );
    virtualTerminalClient->set_object_pool(
        0,
        object_pool_start,
        (object_pool_end - object_pool_start),
        "1200"   // Pool designator - change this if you update the pool
    );
    virtualTerminalClient->get_vt_soft_key_event_dispatcher()
                          .add_listener(handle_softkey_event);
    virtualTerminalClient->get_vt_button_event_dispatcher()
                          .add_listener(handle_button_event);
    virtualTerminalClient->initialize(true);

    virtualTerminalUpdateHelper =
        std::make_shared<isobus::VirtualTerminalClientUpdateHelper>(
            virtualTerminalClient
        );

    // Track all numeric values we will update at runtime
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_FoldStep,    0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_UnfoldStep,  0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_FanRPMTarget,  FAN_RPM_DEFAULT);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_FanRPMActual,  0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_VacTarget,     VAC_PRESSURE_DEFAULT);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_VacActual,     0);
    virtualTerminalUpdateHelper->initialize();

    // ── MODULE INITIALIZATION ─────────────────────────────────────────────────
    fold_sequence_init();   // Initialize I2C, MCP23017s, all solenoids off
    plant_control_init();   // Initialize plant mode, all solenoids off
    fan_vac_init();         // Initialize PWM, RPM sensor, ADC, PID controllers
    fan_vac_start();        // Start fan and vac control loops

    // ── PID UPDATE TIMER ──────────────────────────────────────────────────────
    // Fires every PID_UPDATE_INTERVAL ms to update fan and vac control
    pidUpdateTimer = xTimerCreate(
        "PIDTimer",
        pdMS_TO_TICKS(PID_UPDATE_INTERVAL),
        pdTRUE,         // Auto reload
        nullptr,
        pid_timer_callback
    );
    if (pidUpdateTimer != nullptr)
    {
        xTimerStart(pidUpdateTimer, 0);
    }

    // ── MAIN LOOP ─────────────────────────────────────────────────────────────
    // ISOBUS stack runs in background threads
    // S bin sensor polled here in main loop
    while (true)
    {
        // Read S bin sensor and update plant control
        bool sBinEmpty = gpio_get_level((gpio_num_t)PIN_SBIN_SENSOR) == 0;
        plant_control_update_sbin(sBinEmpty);

        // Small delay - ISOBUS stack handles its own timing
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // ── CLEANUP (never reached in normal operation) ───────────────────────────
    virtualTerminalClient->terminate();
    isobus::CANHardwareInterface::stop();
}