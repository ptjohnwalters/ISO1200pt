#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/mcp2515_can_interface.hpp"
#include "isobus/hardware_integration/spi_interface_esp.hpp"
#include "isobus/isobus/can_NAME_filter.hpp"
#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/can_internal_control_function.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client_update_helper.hpp"

#include "console_logger.cpp"
#include "fan_vac_control.hpp"
#include "fold_sequence.hpp"
#include "objectPoolObjects.h"
#include "plant_control.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

// ─── HARDWARE CONFIGURATION ─────────────────────────────────────────────────
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

// ─── ECU NAME CONFIGURATION ────────────────────────────────────────────────
constexpr std::uint16_t ECU_MANUFACTURER_CODE = 1407;
constexpr std::uint32_t ECU_IDENTITY_NUMBER = 242860;

// ─── GLOBAL STATE ───────────────────────────────────────────────────────────
static std::shared_ptr<isobus::VirtualTerminalClient> virtualTerminalClient = nullptr;
static std::shared_ptr<isobus::VirtualTerminalClientUpdateHelper> virtualTerminalUpdateHelper = nullptr;
static std::shared_ptr<isobus::InternalControlFunction> internalECU = nullptr;
static std::shared_ptr<isobus::PartneredControlFunction> partneredVT = nullptr;
static std::shared_ptr<isobus::SPIInterfaceESP> spiInterface = nullptr;

// Screen tracking
enum class ActiveScreen : uint8_t
{
    HOME,
    UNFOLD,
    PLANT,
    FAN_VAC
};

static ActiveScreen currentScreen = ActiveScreen::HOME;

// ─── HELPER FUNCTIONS ───────────────────────────────────────────────────────
void update_fan_vac_display_values()
{
    if (!virtualTerminalUpdateHelper)
    {
        return;
    }

    uint32_t fanRPM = fan_vac_get_fan_speed_rpm();
    uint32_t vacPressure = fan_vac_get_vac_pressure_in_h2o();

    virtualTerminalUpdateHelper->set_numeric_value(Value_FanSpeedOutput, fanRPM);
    virtualTerminalUpdateHelper->set_numeric_value(Value_VacPressureOutput, vacPressure);
}

void update_fold_display_values()
{
    if (!virtualTerminalUpdateHelper)
    {
        return;
    }

    uint32_t currentStep = fold_sequence_get_current_step();
    uint32_t totalSteps = fold_sequence_get_total_steps();

    virtualTerminalUpdateHelper->set_numeric_value(Value_UnfoldCurrentStep, currentStep + 1);
    virtualTerminalUpdateHelper->set_numeric_value(Value_UnfoldTotalSteps, totalSteps);
}

void update_plant_display_values()
{
    if (!virtualTerminalUpdateHelper)
    {
        return;
    }

    uint32_t mode = static_cast<uint32_t>(plant_control_get_mode());
    virtualTerminalUpdateHelper->set_numeric_value(Value_PlantModeIndicator, mode);
}

void show_screen(ActiveScreen screen)
{
    if (!virtualTerminalUpdateHelper)
    {
        return;
    }

    switch (screen)
    {
        case ActiveScreen::HOME:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Home
            );
            break;

        case ActiveScreen::UNFOLD:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Unfold
            );
            update_fold_display_values();
            break;

        case ActiveScreen::PLANT:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_Plant
            );
            update_plant_display_values();
            break;

        case ActiveScreen::FAN_VAC:
            virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(
                WorkingSet_1200PT,
                DataMask_FanVac
            );
            update_fan_vac_display_values();
            break;
    }

    currentScreen = screen;
}

// ─── VT EVENT CALLBACKS ─────────────────────────────────────────────────────
void handle_softkey_event(const isobus::VirtualTerminalClient::VTKeyEvent &event)
{
    if (event.keyEvent != isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased)
    {
        return;
    }

    switch (event.objectID)
    {
        case SoftKey_Home:
            show_screen(ActiveScreen::HOME);
            break;

        case SoftKey_Unfold:
            show_screen(ActiveScreen::UNFOLD);
            break;

        case SoftKey_Plant:
            show_screen(ActiveScreen::PLANT);
            break;

        case SoftKey_FanVac:
            show_screen(ActiveScreen::FAN_VAC);
            break;

        default:
            break;
    }
}

void handle_button_event(const isobus::VirtualTerminalClient::VTKeyEvent &event)
{
    if ((event.keyEvent != isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased) &&
        (event.keyEvent != isobus::VirtualTerminalClient::KeyActivationCode::ButtonStillHeld))
    {
        return;
    }

    switch (event.objectID)
    {
        // ── HOME SCREEN BUTTONS ───────────────────────────────────────────────
        case Button_HomeToUnfold:
            show_screen(ActiveScreen::UNFOLD);
            break;

        case Button_HomeToPlant:
            show_screen(ActiveScreen::PLANT);
            break;

        case Button_HomeToFanVac:
            show_screen(ActiveScreen::FAN_VAC);
            break;

        // ── UNFOLD SCREEN BUTTONS ─────────────────────────────────────────────
        case Button_UnfoldStart:
            fold_sequence_start();
            update_fold_display_values();
            break;

        case Button_UnfoldNext:
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
                else
                {
                    update_fold_display_values();
                }
            }
            break;

        case Button_UnfoldPrev:
            fold_sequence_prev_step();
            update_fold_display_values();
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
            update_plant_display_values();
            break;

        case Button_PlantFullLift:
            plant_control_set_full_lift();
            update_plant_display_values();
            break;

        case Button_PlantLower:
            plant_control_set_lowered();
            update_plant_display_values();
            break;

        // ── FAN VAC BUTTONS ───────────────────────────────────────────────────
        case Button_FanSpeedUp:
            fan_vac_fan_speed_up();
            update_fan_vac_display_values();
            break;

        case Button_FanSpeedDown:
            fan_vac_fan_speed_down();
            update_fan_vac_display_values();
            break;

        case Button_VacPressureUp:
            fan_vac_vac_pressure_up();
            update_fan_vac_display_values();
            break;

        case Button_VacPressureDown:
            fan_vac_vac_pressure_down();
            update_fan_vac_display_values();
            break;

        default:
            break;
    }
}

// ─── MAIN ENTRY POINT ───────────────────────────────────────────────────────
extern "C" const std::uint8_t object_pool_start[] asm("_binary_object_pool_iop_start");
extern "C" const std::uint8_t object_pool_end[] asm("_binary_object_pool_iop_end");

extern "C" void app_main()
{
    constexpr auto TAG = "ISO1200PT_MCP2515";

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

    std::shared_ptr<isobus::CANHardwarePlugin> canDriver =
        std::make_shared<isobus::MCP2515CANInterface>(
            spiInterface.get(),
            MCP2515_250K_8MHZ_CNF1,
            MCP2515_250K_8MHZ_CNF2,
            MCP2515_250K_8MHZ_CNF3
        );

    ESP_LOGI(TAG, "Using MCP2515 bit timing CNF1=0x%02X CNF2=0x%02X CNF3=0x%02X",
             MCP2515_250K_8MHZ_CNF1,
             MCP2515_250K_8MHZ_CNF2,
             MCP2515_250K_8MHZ_CNF3);

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
    isobus::NAME planterECUName(0);
    planterECUName.set_arbitrary_address_capable(true);
    planterECUName.set_industry_group(1);
    planterECUName.set_device_class(0);
    planterECUName.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::PlanterOrSeeder));
    planterECUName.set_identity_number(ECU_IDENTITY_NUMBER);
    planterECUName.set_ecu_instance(0);
    planterECUName.set_function_instance(0);
    planterECUName.set_device_class_instance(0);
    planterECUName.set_manufacturer_code(ECU_MANUFACTURER_CODE);

    internalECU = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(
        planterECUName,
        0
    );

    const isobus::NAMEFilter vtFilter(
        isobus::NAME::NAMEParameters::FunctionCode,
        static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal)
    );

    partneredVT = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(
        0,
        {vtFilter}
    );

    if ((!internalECU) || (!partneredVT))
    {
        ESP_LOGE("ISO1200PT", "Failed to create ECU or VT partner control function");
        isobus::CANHardwareInterface::stop();
        return;
    }

    // ── VT CLIENT SETUP ───────────────────────────────────────────────────────
    virtualTerminalClient = std::make_shared<isobus::VirtualTerminalClient>(
        partneredVT,
        internalECU
    );

    const std::uint8_t *objectPoolData = object_pool_start;
    const std::size_t objectPoolSize = static_cast<std::size_t>(object_pool_end - object_pool_start);

    if (!virtualTerminalClient->set_object_pool(0, objectPoolData, objectPoolSize, "1200PT"))
    {
        ESP_LOGE("ISO1200PT", "Failed to set VT object pool");
        isobus::CANHardwareInterface::stop();
        return;
    }

    virtualTerminalClient->get_vt_soft_key_event_dispatcher().add_listener(handle_softkey_event);
    virtualTerminalClient->get_vt_button_event_dispatcher().add_listener(handle_button_event);

    if (!virtualTerminalClient->initialize(true))
    {
        ESP_LOGE("ISO1200PT", "Failed to initialize VT client");
        isobus::CANHardwareInterface::stop();
        return;
    }

    virtualTerminalUpdateHelper = std::make_shared<isobus::VirtualTerminalClientUpdateHelper>(
        virtualTerminalClient
    );

    virtualTerminalUpdateHelper->initialize();

    // ── FEATURE MODULE INITIALIZATION ─────────────────────────────────────────
    fold_sequence_init();
    plant_control_init();
    fan_vac_control_init();

    // ── START ON HOME SCREEN ──────────────────────────────────────────────────
    show_screen(ActiveScreen::HOME);

    ESP_LOGI("ISO1200PT", "System initialized successfully");

    // ── MAIN LOOP ─────────────────────────────────────────────────────────────
    while (true)
    {
        fan_vac_update();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
