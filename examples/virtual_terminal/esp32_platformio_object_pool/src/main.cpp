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

#include "console_logger.cpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "objectPoolObjects.h"

#include <functional>
#include <iostream>
#include <memory>

//! It is discouraged to use global variables, but it is done here for simplicity.
static std::shared_ptr<isobus::VirtualTerminalClient> virtualTerminalClient = nullptr;
static std::shared_ptr<isobus::VirtualTerminalClientUpdateHelper> virtualTerminalUpdateHelper = nullptr;
static std::shared_ptr<isobus::SPIInterfaceESP> spiInterface = nullptr;

// This callback will provide us with event driven notifications of softkey presses from the stack
void handle_softkey_event(const isobus::VirtualTerminalClient::VTKeyEvent &event)
{
	if (event.keyNumber == 0)
	{
		// We have the alarm ACK code, so if we have an active alarm, acknowledge it by going back to the main runscreen
		virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(example_WorkingSet, mainRunscreen_DataMask);
	}

	switch (event.keyEvent)
	{
		case isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased:
		{
			switch (event.objectID)
			{
				case alarm_SoftKey:
				{
					virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(example_WorkingSet, example_AlarmMask);
				}
				break;

				case acknowledgeAlarm_SoftKey:
				{
					virtualTerminalUpdateHelper->set_active_data_or_alarm_mask(example_WorkingSet, mainRunscreen_DataMask);
				}
				break;

				default:
					break;
			}
		}
		break;

		default:
			break;
	}
}

// This callback will provide us with event driven notifications of button presses from the stack
void handle_button_event(const isobus::VirtualTerminalClient::VTKeyEvent &event)
{
	switch (event.keyEvent)
	{
		case isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased:
		case isobus::VirtualTerminalClient::KeyActivationCode::ButtonStillHeld:
		{
			switch (event.objectID)
			{
				case Plus_Button:
				{
					virtualTerminalUpdateHelper->increase_numeric_value(ButtonExampleNumber_VarNum);
				}
				break;

				case Minus_Button:
				{
					virtualTerminalUpdateHelper->decrease_numeric_value(ButtonExampleNumber_VarNum);
				}
				break;

				default:
					break;
			}
		}
		break;

		default:
			break;
	}
}

extern "C" const std::uint8_t object_pool_start[] asm("_binary_object_pool_iop_start");
extern "C" const std::uint8_t object_pool_end[] asm("_binary_object_pool_iop_end");

namespace
{
	static constexpr gpio_num_t MCP2515_SPI_SCK = GPIO_NUM_18;
	static constexpr gpio_num_t MCP2515_SPI_MOSI = GPIO_NUM_23;
	static constexpr gpio_num_t MCP2515_SPI_MISO = GPIO_NUM_19;
	static constexpr gpio_num_t MCP2515_SPI_CS = GPIO_NUM_5;
	static constexpr gpio_num_t MCP2515_INT = GPIO_NUM_4;
	static constexpr spi_host_device_t MCP2515_SPI_HOST = VSPI_HOST;

	static constexpr std::uint8_t MCP2515_250K_8MHZ_CNF1 = 0x80;
	static constexpr std::uint8_t MCP2515_250K_8MHZ_CNF2 = 0xE5;
	static constexpr std::uint8_t MCP2515_250K_8MHZ_CNF3 = 0x83;
}

extern "C" void app_main()
{
	constexpr auto TAG = "VT_MCP2515";

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

	std::shared_ptr<isobus::CANHardwarePlugin> canDriver = std::make_shared<isobus::MCP2515CANInterface>(spiInterface.get(),
	                                                                                                      MCP2515_250K_8MHZ_CNF1,
	                                                                                                      MCP2515_250K_8MHZ_CNF2,
	                                                                                                      MCP2515_250K_8MHZ_CNF3);
	ESP_LOGI(TAG, "Using MCP2515 bit timing CNF1=0x%02X CNF2=0x%02X CNF3=0x%02X", MCP2515_250K_8MHZ_CNF1, MCP2515_250K_8MHZ_CNF2, MCP2515_250K_8MHZ_CNF3);

	isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
	isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Info); // Change this to Debug to see more information
	isobus::CANHardwareInterface::set_number_of_can_channels(1);
	isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, canDriver);
	// isobus::CANHardwareInterface::set_periodic_update_interval(10); // 10ms update period matches the default FreeRTOS tick rate of 100Hz

	if (!isobus::CANHardwareInterface::start() || !canDriver->get_is_valid())
	{
		ESP_LOGE("AgIsoStack", "Failed to start hardware interface, the CAN driver might be invalid");
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

	isobus::NAME TestDeviceNAME(0);

	//! Make sure you change these for your device!!!!
	TestDeviceNAME.set_arbitrary_address_capable(true);
	TestDeviceNAME.set_industry_group(1);
	TestDeviceNAME.set_device_class(0);
	TestDeviceNAME.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::SteeringControl));
	TestDeviceNAME.set_identity_number(2);
	TestDeviceNAME.set_ecu_instance(0);
	TestDeviceNAME.set_function_instance(0);
	TestDeviceNAME.set_device_class_instance(0);
	TestDeviceNAME.set_manufacturer_code(1407);

	const std::uint8_t *testPool = object_pool_start;

	const isobus::NAMEFilter filterVirtualTerminal(isobus::NAME::NAMEParameters::FunctionCode, static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal));
	const std::vector<isobus::NAMEFilter> vtNameFilters = { filterVirtualTerminal };
	auto TestInternalECU = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(TestDeviceNAME, 0);
	auto TestPartnerVT = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(0, vtNameFilters);

	virtualTerminalClient = std::make_shared<isobus::VirtualTerminalClient>(TestPartnerVT, TestInternalECU);
	virtualTerminalClient->set_object_pool(0, testPool, (object_pool_end - object_pool_start), "ais1");
	virtualTerminalClient->get_vt_soft_key_event_dispatcher().add_listener(handle_softkey_event);
	virtualTerminalClient->get_vt_button_event_dispatcher().add_listener(handle_button_event);
	virtualTerminalClient->initialize(true);

	virtualTerminalUpdateHelper = std::make_shared<isobus::VirtualTerminalClientUpdateHelper>(virtualTerminalClient);
	virtualTerminalUpdateHelper->add_tracked_numeric_value(ButtonExampleNumber_VarNum, 214748364); // In the object pool the output number has an offset of -214748364 so we use this to represent 0.
	virtualTerminalUpdateHelper->initialize();

	while (true)
	{
		// CAN stack runs in other threads. Do nothing forever.
		vTaskDelay(10);
	}

	virtualTerminalClient->terminate();
	isobus::CANHardwareInterface::stop();
}
