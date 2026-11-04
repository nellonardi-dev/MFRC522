/*
 * Basic example for MCRF522 library
 * This example demonstrates how to initialize and use the MFRC522 RFID reader
 */

#include <SPI.h>
#include <RC522_Driver.h>

// Define pin connections
#define RC522_SS_PIN   10
#define RC522_RST_PIN  9

// Create instance of the RC522 driver
RC522_Driver reader(RC522_SS_PIN, RC522_RST_PIN);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  
  // Initialize the RFID module
  reader.begin();
  
  // Test communication and version detection
  uint8_t version = reader.getVersion();
  if (version == 0x90 || version == 0x91 || version == 0x92) {
    Serial.println("MFRC522 detected");
  } else {
    Serial.println("Error: MFRC522 not found or wrong version");
  }
}

void loop() {
  // Detect cards with REQA command
  const uint8_t request = PICC_CMD_REQA;
  uint8_t response[8] = {0};
  uint8_t responseLength = 0;
  
  StatusCode status = reader.transceiveData(
    &request,
    1,
    response,
    &responseLength,
    sizeof(response),
    7,          // REQA uses 7 bits
    0,
    nullptr,
    nullptr,
    false
  );
  
  if (status == STATUS_OK && responseLength >= 2) {
    Serial.print("Card detected! ATQA: ");
    Serial.print(response[0], HEX);
    Serial.print(" ");
    Serial.println(response[1], HEX);
  }
  
  delay(1000);
}