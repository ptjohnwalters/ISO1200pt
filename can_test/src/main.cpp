#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

// ESP32 VSPI pins
static constexpr uint8_t CAN_CS = 5;
static constexpr uint8_t CAN_INT = 4;
static constexpr uint8_t SPI_SCK = 18;
static constexpr uint8_t SPI_MISO = 19;
static constexpr uint8_t SPI_MOSI = 23;

MCP_CAN CAN0(CAN_CS);

bool initializeCan() {
  // Most MCP2515 boards use either an 8 MHz or 16 MHz crystal.
  if (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN init OK @250k (8MHz)");
    return true;
  }

  // If the first attempt failed, retry for a 16 MHz module.
  if (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("CAN init OK @250k (16MHz)");
    return true;
  }

  Serial.println("CAN init FAILED");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("MCP2515 CAN listener starting...");

  pinMode(CAN_INT, INPUT);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, CAN_CS);

  if (!initializeCan()) {
    Serial.println("Check VCC, GND, SPI wiring, CS, and the MCP2515 crystal setting.");
    while (true) {
      delay(1000);
    }
  }

  CAN0.setMode(MCP_NORMAL);
  Serial.println("Listening...");
}

void loop() {
  if (digitalRead(CAN_INT) == LOW) {
    unsigned long id = 0;
    uint8_t length = 0;
    uint8_t data[8] = {0};

    if (CAN0.readMsgBuf(&id, &length, data) == CAN_OK) {
      Serial.print("ID: 0x");
      Serial.print(id, HEX);
      Serial.print("  LEN: ");
      Serial.print(length);
      Serial.print("  DATA:");

      for (uint8_t i = 0; i < length; ++i) {
        Serial.print(' ');
        if (data[i] < 0x10) {
          Serial.print('0');
        }
        Serial.print(data[i], HEX);
      }

      Serial.println();
    }
  }

  delay(1);
}
