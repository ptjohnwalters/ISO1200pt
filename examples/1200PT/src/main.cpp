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
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "objectPoolObjects.h"
#include "fold_sequence.h"
#include "plant_control.h"
#include "fan_vac_control.h"

#include <functional>
#include <iostream>
#include <memory>

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

// ─── PID UPDATE TIMER ────────────────────────────────────────────────────────────────
static TimerHandle_t pidUpdateTimer = nullptr;

// ─── SCREEN TRACKING ─────────────────────────────────────────────────────────────────
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

// ─── FORWARD DECLARATIONS ──────────────────────────────────────────────────────────────
void update_display_values();
void handle_softkey_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);
void handle_button_event(const isobus::VirtualTerminalClient::VTKeyEvent &event);

// ─── PID TIMER CALLBACK ─────────────────────────────────────────────────────────────
// Called every PID_UPDATE_INTERVAL milliseconds
// Updates fan and vac PID loops and refreshes display values
static void pid_timer_callback(TimerHandle_t xTimer)
{
    fan_vac_update();
    update_display_values();
}

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
    if (fold_sequence_get_state() == SequenceState::FOLD_ACTIVE ||
        fold_sequence_get_state() == SequenceState::UNFOLD_ACTIVE)
    {
        virtualTerminalUpdateHelper->set_numeric_value(
            VarNum_FoldStep,
            fold_sequence_get_current_step()
        );
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
        // ── HOME SCREEN NAVIGATION ───────────────────────────────────────────────
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

        // ── FOLD SEQUENCE BUTTONS ───────────────────────────────────────────────────
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

        // ── UNFOLD SEQUENCE BUTTONS ──────────────────────────────────────────────────
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

    // ── MODULE INITIALIZATION ─────────────────────────────────────────────────────
    fold_sequence_init();   // Initialize I2C, MCP23017s, all solenoids off
    plant_control_init();   // Initialize plant mode, all solenoids off
    fan_vac_init();         // Initialize PWM, RPM sensor, ADC, PID controllers
    fan_vac_start();        // Start fan and vac control loops

    // ── PID UPDATE TIMER ────────────────────────────────────────────────────────────────
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

    ESP_LOGI("ISO1200PT", "System initialized successfully");

    // ── MAIN LOOP ─────────────────────��────────────────────────────────────────
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
