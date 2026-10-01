#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/mcp2515_can_interface.hpp"
#include "isobus/hardware_integration/spi_interface_esp.hpp"
#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client_update_helper.hpp"
#include "isobus/utility/iop_file_interface.hpp"

#include "console_logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "objectPoolObjects.h"
#include "fold_sequence.h"
#include "plant_control.h"
#include "fan_vac_control.h"

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

// ─── ISOBUS CLIENT INSTANCES ───────────────────────────────────────────
static std::shared_ptr<isobus::VirtualTerminalClient> virtualTerminalClient = nullptr;
static std::shared_ptr<isobus::VirtualTerminalClientUpdateHelper> virtualTerminalUpdateHelper = nullptr;
static std::shared_ptr<isobus::SPIInterfaceESP> spiInterface = nullptr;

// ─── MCP2515 SPI CAN PIN DEFINITIONS ───────────────────────────────────────
namespace
{
    constexpr gpio_num_t MCP2515_SPI_SCK = GPIO_NUM_18;
    constexpr gpio_num_t MCP2515_SPI_MOSI = GPIO_NUM_23;
    constexpr gpio_num_t MCP2515_SPI_MISO = GPIO_NUM_19;
    constexpr gpio_num_t MCP2515_SPI_CS = GPIO_NUM_5;
    constexpr gpio_num_t MCP2515_INT = GPIO_NUM_4;
    constexpr spi_host_device_t MCP2515_SPI_HOST = VSPI_HOST;

    // 250 kbit/s @ 8 MHz MCP2515 oscillator
    constexpr std::uint8_t MCP2515_250K_8MHZ_CNF1 = 0x80;
    constexpr std::uint8_t MCP2515_250K_8MHZ_CNF2 = 0xE5;
    constexpr std::uint8_t MCP2515_250K_8MHZ_CNF3 = 0x83;
}

// ─── SCREEN TRACKING ─────────────────────────────────────────────────────────────────
// Tracks which screen is currently active on InCommand
enum class ActiveScreen
{
    RUN,
    UNFOLD,
    FOLD,
    CAL
};
static ActiveScreen currentScreen = ActiveScreen::RUN;
static std::atomic<bool> hydraulicActuationInhibited{true};

// ─── MARKER TRACKING ─────────────────────────────────────────────────────────────────
enum class MarkerState
{
    OFF,
    LEFT,
    RIGHT
};
static MarkerState markerState = MarkerState::OFF;

// ─── CALIBRATION STATE ───────────────────────────────────────────────────────────────
static uint32_t calPosition = 50;
static uint32_t calTransportLimit = 95;
static uint32_t calPlantLimit = 15;
static uint32_t calSensorRaw = 2048;

// ─── FORWARD DECLARATIONS ──────────────────────────────────────────────────────────────
void update_display_values();
void handle_softkey_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);
void handle_button_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);
void reset_action_buttons();
void handle_unfold_action_button(uint8_t actionIndex);
void handle_fold_action_button(uint8_t actionIndex);

// ─── DISPLAY UPDATE FUNCTION ──────────────────────────────────────────────────────────
// Pushes current sensor values and status to InCommand display
void update_display_values()
{
    if (virtualTerminalUpdateHelper == nullptr) return;

    FanVacStatus fanVacStatus = fan_vac_get_status();

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
    if (fold_sequence_get_state() == SequenceState::FOLD_ACTIVE)
    {
        uint8_t step = fold_sequence_get_active_action();
        virtualTerminalUpdateHelper->set_numeric_value(
            VarNum_FoldStep,
            step
        );
        calPosition = 15 + ((step * 80) / FOLD_STEPS);
        calSensorRaw = (calPosition * 4095) / 100;
        virtualTerminalUpdateHelper->set_numeric_value(VarNum_CalPosition, calPosition);
        virtualTerminalUpdateHelper->set_numeric_value(VarNum_CalSensorRaw, calSensorRaw);
    }
    else if (fold_sequence_get_state() == SequenceState::UNFOLD_ACTIVE)
    {
        uint8_t step = fold_sequence_get_active_action();
        virtualTerminalUpdateHelper->set_numeric_value(
            VarNum_UnfoldStep,
            step
        );
        calPosition = 95 - ((step * 80) / UNFOLD_STEPS);
        calSensorRaw = (calPosition * 4095) / 100;
        virtualTerminalUpdateHelper->set_numeric_value(VarNum_CalPosition, calPosition);
        virtualTerminalUpdateHelper->set_numeric_value(VarNum_CalSensorRaw, calSensorRaw);
    }

    if (virtualTerminalClient != nullptr)
    {
        const char *stateStr = "MID RANGE";
        if (calPosition >= calTransportLimit - 5)
        {
            stateStr = "TRANSPORT RANGE";
        }
        else if (calPosition <= calPlantLimit + 5)
        {
            stateStr = "PLANT RANGE";
        }
        virtualTerminalClient->send_change_string_value(VarStr_CalPositionState, stateStr);
    }
}

// ─── HYDRAULIC ACTION BUTTON HELPERS ────────────────────────────────────────────────
static uint8_t activeUnfoldAction = 0;
static uint8_t activeFoldAction = 0;

static const uint16_t UNFOLD_BTN_IDS[6] = {
    Button_UnfoldAction1, Button_UnfoldAction2, Button_UnfoldAction3,
    Button_UnfoldAction4, Button_UnfoldAction5, Button_UnfoldAction6
};
static const uint16_t UNFOLD_STR_IDS[6] = {
    VarStr_UnfoldAction1, VarStr_UnfoldAction2, VarStr_UnfoldAction3,
    VarStr_UnfoldAction4, VarStr_UnfoldAction5, VarStr_UnfoldAction6
};

static const uint16_t FOLD_BTN_IDS[6] = {
    Button_FoldAction1, Button_FoldAction2, Button_FoldAction3,
    Button_FoldAction4, Button_FoldAction5, Button_FoldAction6
};
static const uint16_t FOLD_STR_IDS[6] = {
    VarStr_FoldAction1, VarStr_FoldAction2, VarStr_FoldAction3,
    VarStr_FoldAction4, VarStr_FoldAction5, VarStr_FoldAction6
};

void reset_action_buttons()
{
    if (virtualTerminalClient != nullptr)
    {
        if (activeUnfoldAction >= 1 && activeUnfoldAction <= 6)
        {
            uint8_t idx = activeUnfoldAction - 1;
            virtualTerminalClient->send_change_string_value(UNFOLD_STR_IDS[idx], "ACTIVATE");
            virtualTerminalClient->send_change_background_colour(UNFOLD_BTN_IDS[idx], 2); // Green
            virtualTerminalClient->send_change_background_colour(UNFOLD_STR_IDS[idx], 2);
        }
        if (activeFoldAction >= 1 && activeFoldAction <= 6)
        {
            uint8_t idx = activeFoldAction - 1;
            virtualTerminalClient->send_change_string_value(FOLD_STR_IDS[idx], "ACTIVATE");
            virtualTerminalClient->send_change_background_colour(FOLD_BTN_IDS[idx], 2); // Green
            virtualTerminalClient->send_change_background_colour(FOLD_STR_IDS[idx], 2);
        }
    }
    activeUnfoldAction = 0;
    activeFoldAction = 0;
}

void handle_unfold_action_button(uint8_t actionIndex)
{
    if (actionIndex < 1 || actionIndex > 6) return;
    if (hydraulicActuationInhibited.load()) return;

    if (activeFoldAction != 0)
    {
        reset_action_buttons();
        fold_sequence_cancel();
    }

    if (activeUnfoldAction == actionIndex)
    {
        // Safe-stop toggle: pressing active action de-energizes it
        fold_sequence_cancel();
        uint8_t idx = actionIndex - 1;
        if (virtualTerminalClient != nullptr)
        {
            virtualTerminalClient->send_change_string_value(UNFOLD_STR_IDS[idx], "ACTIVATE");
            virtualTerminalClient->send_change_background_colour(UNFOLD_BTN_IDS[idx], 2); // Green
            virtualTerminalClient->send_change_background_colour(UNFOLD_STR_IDS[idx], 2);
            virtualTerminalClient->send_change_string_value(
                VarStr_UnfoldInstruction,
                "HYDRAULICS SAFE - IDLE"
            );
        }
        if (virtualTerminalUpdateHelper != nullptr)
        {
            virtualTerminalUpdateHelper->set_numeric_value(VarNum_UnfoldStep, 0);
        }
        activeUnfoldAction = 0;
    }
    else
    {
        // De-energize previous action button display if active
        if (activeUnfoldAction != 0 && virtualTerminalClient != nullptr)
        {
            uint8_t prevIdx = activeUnfoldAction - 1;
            virtualTerminalClient->send_change_string_value(UNFOLD_STR_IDS[prevIdx], "ACTIVATE");
            virtualTerminalClient->send_change_background_colour(UNFOLD_BTN_IDS[prevIdx], 2); // Green
            virtualTerminalClient->send_change_background_colour(UNFOLD_STR_IDS[prevIdx], 2);
        }

        // Activate new action (calls fold_sequence_all_off internally to guarantee mutual exclusion)
        fold_sequence_activate_unfold_action(actionIndex);
        activeUnfoldAction = actionIndex;

        uint8_t idx = actionIndex - 1;
        if (virtualTerminalClient != nullptr)
        {
            virtualTerminalClient->send_change_string_value(UNFOLD_STR_IDS[idx], "STOP");
            virtualTerminalClient->send_change_background_colour(UNFOLD_BTN_IDS[idx], 12); // Red
            virtualTerminalClient->send_change_background_colour(UNFOLD_STR_IDS[idx], 12);
            virtualTerminalClient->send_change_string_value(
                VarStr_UnfoldInstruction,
                fold_sequence_get_instruction()
            );
        }
        if (virtualTerminalUpdateHelper != nullptr)
        {
            virtualTerminalUpdateHelper->set_numeric_value(VarNum_UnfoldStep, actionIndex);
        }
    }
}

void handle_fold_action_button(uint8_t actionIndex)
{
    if (actionIndex < 1 || actionIndex > 6) return;
    if (hydraulicActuationInhibited.load()) return;

    if (activeUnfoldAction != 0)
    {
        reset_action_buttons();
        fold_sequence_cancel();
    }

    if (activeFoldAction == actionIndex)
    {
        // Safe-stop toggle: pressing active action de-energizes it
        fold_sequence_cancel();
        uint8_t idx = actionIndex - 1;
        if (virtualTerminalClient != nullptr)
        {
            virtualTerminalClient->send_change_string_value(FOLD_STR_IDS[idx], "ACTIVATE");
            virtualTerminalClient->send_change_background_colour(FOLD_BTN_IDS[idx], 2); // Green
            virtualTerminalClient->send_change_background_colour(FOLD_STR_IDS[idx], 2);
            virtualTerminalClient->send_change_string_value(
                VarStr_FoldInstruction,
                "HYDRAULICS SAFE - IDLE"
            );
        }
        if (virtualTerminalUpdateHelper != nullptr)
        {
            virtualTerminalUpdateHelper->set_numeric_value(VarNum_FoldStep, 0);
        }
        activeFoldAction = 0;
    }
    else
    {
        // De-energize previous action button display if active
        if (activeFoldAction != 0 && virtualTerminalClient != nullptr)
        {
            uint8_t prevIdx = activeFoldAction - 1;
            virtualTerminalClient->send_change_string_value(FOLD_STR_IDS[prevIdx], "ACTIVATE");
            virtualTerminalClient->send_change_background_colour(FOLD_BTN_IDS[prevIdx], 2); // Green
            virtualTerminalClient->send_change_background_colour(FOLD_STR_IDS[prevIdx], 2);
        }

        // Activate new action (calls fold_sequence_all_off internally to guarantee mutual exclusion)
        fold_sequence_activate_fold_action(actionIndex);
        activeFoldAction = actionIndex;

        uint8_t idx = actionIndex - 1;
        if (virtualTerminalClient != nullptr)
        {
            virtualTerminalClient->send_change_string_value(FOLD_STR_IDS[idx], "STOP");
            virtualTerminalClient->send_change_background_colour(FOLD_BTN_IDS[idx], 12); // Red
            virtualTerminalClient->send_change_background_colour(FOLD_STR_IDS[idx], 12);
            virtualTerminalClient->send_change_string_value(
                VarStr_FoldInstruction,
                fold_sequence_get_instruction()
            );
        }
        if (virtualTerminalUpdateHelper != nullptr)
        {
            virtualTerminalUpdateHelper->set_numeric_value(VarNum_FoldStep, actionIndex);
        }
    }
}

// ─── SOFTKEY EVENT HANDLER ──────────────────────────────────────────────────────────
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
        case SoftKey_Run:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Run
            );
            currentScreen = ActiveScreen::RUN;
            break;

        case SoftKey_Unfold:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Unfold
            );
            currentScreen = ActiveScreen::UNFOLD;
            break;

        case SoftKey_Fold:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Fold
            );
            currentScreen = ActiveScreen::FOLD;
            break;

        case SoftKey_Cal:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Cal
            );
            currentScreen = ActiveScreen::CAL;
            break;

        default:
            break;
    }
}

// ─── BUTTON EVENT HANDLER ─────────────────────────────────────────────────────────────
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
        // ── TOP TAB NAVIGATION ───────────────────────────────────────────────
        case Button_TabRun:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Run
            );
            currentScreen = ActiveScreen::RUN;
            break;

        case Button_TabUnfold:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Unfold
            );
            currentScreen = ActiveScreen::UNFOLD;
            break;

        case Button_TabFold:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Fold
            );
            currentScreen = ActiveScreen::FOLD;
            break;

        case Button_TabCal:
            fold_sequence_cancel();
            reset_action_buttons();
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Cal
            );
            currentScreen = ActiveScreen::CAL;
            break;

        // ── FOLD ACTION BUTTONS ─────────────────────────────────────────────────────
        case Button_FoldAction1:
            handle_fold_action_button(1);
            break;

        case Button_FoldAction2:
            handle_fold_action_button(2);
            break;

        case Button_FoldAction3:
            handle_fold_action_button(3);
            break;

        case Button_FoldAction4:
            handle_fold_action_button(4);
            break;

        case Button_FoldAction5:
            handle_fold_action_button(5);
            break;

        case Button_FoldAction6:
            handle_fold_action_button(6);
            break;

        // ── UNFOLD ACTION BUTTONS ───────────────────────────────────────────────────
        case Button_UnfoldAction1:
            handle_unfold_action_button(1);
            break;

        case Button_UnfoldAction2:
            handle_unfold_action_button(2);
            break;

        case Button_UnfoldAction3:
            handle_unfold_action_button(3);
            break;

        case Button_UnfoldAction4:
            handle_unfold_action_button(4);
            break;

        case Button_UnfoldAction5:
            handle_unfold_action_button(5);
            break;

        case Button_UnfoldAction6:
            handle_unfold_action_button(6);
            break;

        // ── PLANT MODE BUTTONS ─────────────────────────────────────────────────
        case Button_PlantLimitedLift:
            plant_control_set_limited_lift();
            break;

        case Button_PlantFullLift:
            plant_control_set_full_lift();
            break;

        case Button_PlantLower:
            plant_control_set_lowered();
            break;

        // ── BULK FILL (FAN) BUTTONS ───────────────────────────────────────────
        case Button_FanSpeedUp:
            fan_vac_fan_speed_up();
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_FanRPMTarget,
                fan_vac_get_status().fanRPMTarget
            );
            break;

        case Button_FanSpeedDown:
            fan_vac_fan_speed_down();
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_FanRPMTarget,
                fan_vac_get_status().fanRPMTarget
            );
            break;

        case Button_FanPower:
            fan_vac_toggle_fan_power();
            if (virtualTerminalClient != nullptr)
            {
                virtualTerminalClient->send_change_string_value(
                    OutStr_BtnFanPower,
                    fan_vac_is_fan_on() ? "ON" : "OFF"
                );
            }
            break;

        // ── VACUUM BUTTONS ────────────────────────────────────────────────────
        case Button_VacPressureUp:
            fan_vac_vac_pressure_up();
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_VacTarget,
                fan_vac_get_status().vacPressureTarget
            );
            break;

        case Button_VacPressureDown:
            fan_vac_vac_pressure_down();
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_VacTarget,
                fan_vac_get_status().vacPressureTarget
            );
            break;

        case Button_VacPower:
            fan_vac_toggle_vac_power();
            if (virtualTerminalClient != nullptr)
            {
                virtualTerminalClient->send_change_string_value(
                    OutStr_BtnVacPower,
                    fan_vac_is_vac_on() ? "ON" : "OFF"
                );
            }
            break;

        // ── MARKER CONTROL BUTTONS ────────────────────────────────────────────
        case Button_MarkerNext:
        case Button_MarkerToggle:
        {
            if (markerState == MarkerState::OFF)
                markerState = MarkerState::LEFT;
            else if (markerState == MarkerState::LEFT)
                markerState = MarkerState::RIGHT;
            else
                markerState = MarkerState::OFF;

            if (virtualTerminalClient != nullptr)
            {
                const char *mStr = (markerState == MarkerState::LEFT) ? "MARKER: 1" :
                                   (markerState == MarkerState::RIGHT) ? "MARKER: 2" : "MARKER: OFF";
                virtualTerminalClient->send_change_string_value(VarStr_MarkerStatus, mStr);
            }
        }
        break;

        case Button_MarkerPrev:
        {
            if (markerState == MarkerState::OFF)
                markerState = MarkerState::RIGHT;
            else if (markerState == MarkerState::RIGHT)
                markerState = MarkerState::LEFT;
            else
                markerState = MarkerState::OFF;

            if (virtualTerminalClient != nullptr)
            {
                const char *mStr = (markerState == MarkerState::LEFT) ? "MARKER: 1" :
                                   (markerState == MarkerState::RIGHT) ? "MARKER: 2" : "MARKER: OFF";
                virtualTerminalClient->send_change_string_value(VarStr_MarkerStatus, mStr);
            }
        }
        break;

        // ── CALIBRATION BUTTONS ───────────────────────────────────────────────
        case Button_CalSetTransport:
            calTransportLimit = calPosition;
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_CalTransportLimit,
                calTransportLimit
            );
            break;

        case Button_CalSetPlant:
            calPlantLimit = calPosition;
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_CalPlantLimit,
                calPlantLimit
            );
            break;

        case Button_CalZero:
            calPosition = 0;
            calSensorRaw = 0;
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_CalPosition,
                calPosition
            );
            virtualTerminalUpdateHelper->set_numeric_value(
                VarNum_CalSensorRaw,
                calSensorRaw
            );
            break;

        default:
            break;
    }
}

// ─── MAIN ENTRY POINT ───────────────────────────────────────────────────────
extern "C" const std::uint8_t object_pool_start[] asm("_binary_object_pool_iop_start");
extern "C" const std::uint8_t object_pool_end[]   asm("_binary_object_pool_iop_end");

extern "C" void app_main()
{
    constexpr auto TAG = "ISO1200PT_MCP2515";

    // ── MCP2515 SPI CAN HARDWARE SETUP ──────────────────────────────────────────
    ESP_LOGI(TAG, "Configuring MCP2515 over SPI (host=%d, SCK=%d, MOSI=%d, MISO=%d, CS=%d, INT=%d, bitrate=250000, osc=8MHz)",
              static_cast<int>(MCP2515_SPI_HOST),
              static_cast<int>(MCP2515_SPI_SCK),
              static_cast<int>(MCP2515_SPI_MOSI),
              static_cast<int>(MCP2515_SPI_MISO),
              static_cast<int>(MCP2515_SPI_CS),
              static_cast<int>(MCP2515_INT));

    spi_bus_config_t spiBusConfig = {};
    spiBusConfig.mosi_io_num = MCP2515_SPI_MOSI;
    spiBusConfig.miso_io_num = MCP2515_SPI_MISO;
    spiBusConfig.sclk_io_num = MCP2515_SPI_SCK;
    spiBusConfig.quadwp_io_num = -1;
    spiBusConfig.quadhd_io_num = -1;
    spiBusConfig.max_transfer_sz = 16;

    const auto spiBusInitResult = spi_bus_initialize(MCP2515_SPI_HOST, &spiBusConfig, SPI_DMA_CH_AUTO);
    if ((ESP_OK != spiBusInitResult) && (ESP_ERR_INVALID_STATE != spiBusInitResult))
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(spiBusInitResult));
        return;
    }

    gpio_config_t intGpioConfig = {};
    intGpioConfig.pin_bit_mask = (1ULL << MCP2515_INT);
    intGpioConfig.mode = GPIO_MODE_INPUT;
    intGpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
    intGpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    intGpioConfig.intr_type = GPIO_INTR_DISABLE;
    const auto gpioInitResult = gpio_config(&intGpioConfig);
    if (ESP_OK != gpioInitResult)
    {
        ESP_LOGE(TAG, "Failed to configure MCP2515 INT pin: %s", esp_err_to_name(gpioInitResult));
        return;
    }

    spi_device_interface_config_t spiDeviceConfig = {};
    spiDeviceConfig.command_bits = 0;
    spiDeviceConfig.address_bits = 0;
    spiDeviceConfig.dummy_bits = 0;
    spiDeviceConfig.mode = 0;
    spiDeviceConfig.duty_cycle_pos = 128;
    spiDeviceConfig.cs_ena_posttrans = 0;
    spiDeviceConfig.cs_ena_pretrans = 0;
    spiDeviceConfig.clock_speed_hz = 1 * 1000 * 1000;
    spiDeviceConfig.input_delay_ns = 0;
    spiDeviceConfig.spics_io_num = MCP2515_SPI_CS;
    spiDeviceConfig.queue_size = 1;
    spiDeviceConfig.flags = 0;
    spiDeviceConfig.pre_cb = nullptr;
    spiDeviceConfig.post_cb = nullptr;

    spiInterface = std::make_shared<isobus::SPIInterfaceESP>(&spiDeviceConfig, MCP2515_SPI_HOST);
    if (!spiInterface->init())
    {
        ESP_LOGE(TAG, "Failed to initialize SPI device for MCP2515");
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    std::shared_ptr<isobus::CANHardwarePlugin> canDriver = std::make_shared<isobus::MCP2515CANInterface>(
        spiInterface.get(),
        MCP2515_250K_8MHZ_CNF1,
        MCP2515_250K_8MHZ_CNF2,
        MCP2515_250K_8MHZ_CNF3
    );

    ESP_LOGI(TAG, "Using MCP2515 bit timing CNF1=0x%02X CNF2=0x%02X CNF3=0x%02X",
              MCP2515_250K_8MHZ_CNF1, MCP2515_250K_8MHZ_CNF2, MCP2515_250K_8MHZ_CNF3);

    // ── ISOBUS STACK SETUP ────────────────────────────────────────────────────
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(
        isobus::CANStackLogger::LoggingLevel::Info
    );
    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, canDriver);

    if (!isobus::CANHardwareInterface::start() || !canDriver->get_is_valid())
    {
        ESP_LOGE(TAG, "Failed to start CAN hardware interface, the CAN driver might be invalid");
        isobus::CANHardwareInterface::stop();
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    else
    {
        ESP_LOGI(TAG, "MCP2515 CAN interface started in normal mode");
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

    // ── VIRTUAL TERMINAL SETUP ────────────────────────────────────────────────────
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
    const std::size_t objectPoolSize = static_cast<std::size_t>(object_pool_end - object_pool_start);
    std::string objectPoolVersion = isobus::IOPFileInterface::hash_object_pool_to_version(
        object_pool_start,
        objectPoolSize
    );
    objectPoolVersion.resize(7, '0');
    ESP_LOGI(TAG, "Embedded VT object pool: version=%s, size=%u bytes",
             objectPoolVersion.c_str(), static_cast<unsigned int>(objectPoolSize));
    virtualTerminalClient->set_object_pool(
        0,
        object_pool_start,
        static_cast<std::uint32_t>(objectPoolSize),
        objectPoolVersion
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
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_FoldStep,          1);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_UnfoldStep,        1);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_FanRPMTarget,      FAN_RPM_DEFAULT);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_FanRPMActual,      0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_VacTarget,         VAC_PRESSURE_DEFAULT);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_VacActual,         0);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_CalPosition,       calPosition);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_CalTransportLimit, calTransportLimit);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_CalPlantLimit,     calPlantLimit);
    virtualTerminalUpdateHelper->add_tracked_numeric_value(VarNum_CalSensorRaw,      calSensorRaw);
    virtualTerminalUpdateHelper->initialize();

    // ── MODULE INITIALIZATION ─────────────────────────────────────────────────────
    fold_sequence_init();   // Initialize I2C, MCP23017s, all solenoids off
    plant_control_init();   // Initialize plant mode, all solenoids off
    fan_vac_init();         // Initialize PWM, RPM sensor, ADC, PID controllers
    fan_vac_start();        // Start fan and vac control loops

    // ── PERIODIC CONTROL UPDATE ─────────────────────────────────────────────────────────
    ESP_LOGI("ISO1200PT", "System initialized successfully");

    // ── MAIN LOOP ─────────────────────��────────────────────────────────────────
    // Periodic control and VT updates run here, not on the FreeRTOS timer service task.
    while (true)
    {
        fan_vac_update();
        const bool hydraulicControlAvailable = fan_vac_get_state() == FanVacState::RUNNING;
        const bool wasActuationInhibited = hydraulicActuationInhibited.exchange(!hydraulicControlAvailable);
        if (!hydraulicControlAvailable && !wasActuationInhibited)
        {
            fold_sequence_cancel();
            reset_action_buttons();
        }
        update_display_values();

        // Read S bin sensor and update plant control
        bool sBinEmpty = gpio_get_level((gpio_num_t)PIN_SBIN_SENSOR) == 0;
        plant_control_update_sbin(sBinEmpty);

        vTaskDelay(pdMS_TO_TICKS(PID_UPDATE_INTERVAL));
    }

    // ── CLEANUP (never reached in normal operation) ───────────────────────────
    virtualTerminalClient->terminate();
    isobus::CANHardwareInterface::stop();
}
