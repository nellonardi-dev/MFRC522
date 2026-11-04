# MCRF522 Arduino Library

This is an Arduino library for the MFRC522 RFID reader module. It provides full support for communication with various types of RFID cards and tags, including MIFARE Classic, Ultralight, and ISO/IEC 14443A compatible cards.

## Features

- Full compatibility with MFRC522 chip (versions 0.0, 1.0, 2.0)
- Support for Fudan Semiconductor FM17522 clones
- Complete implementation of all RF communication registers  
- Self-test functionality with version detection
- CRC calculation and verification capabilities
- Antenna gain control and modulation configuration
- Baud rate setting for different communication speeds
- Support for multiple SPI interfaces

## Hardware Requirements

- MFRC522 RFID module (or compatible clone)
- Arduino board (Uno, Mega, ESP32, etc.)
- SPI connection between Arduino and MFRC522:
  - MOSI → Pin 11 (Arduino Uno) or GPIO 23 (ESP32)
  - MISO → Pin 12 (Arduino Uno) or GPIO 19 (ESP32)  
  - SCK  → Pin 13 (Arduino Uno) or GPIO 18 (ESP32)
  - SS   → Pin 10 (or any digital pin, Arduino Uno) or GPIO 5 (ESP32)
  - RST  → Pin 9 (or any digital pin) 
- Power supply: 3.3V (MFRC522 requires 3.3V logic levels)

## Installation

### Using Arduino IDE Library Manager
1. Open Arduino IDE
2. Go to Sketch > Include Library > Manage Libraries...
3. Search for "MCRF522"
4. Install the library

### Manual Installation
```bash
cd ~/Arduino/libraries/
git clone https://github.com/nello-nardi/MCRF522.git
```

## Quick Start Example

```cpp
#include <SPI.h>
#include <RC522_Driver.h>

#define RC522_SS_PIN   10
#define RC522_RST_PIN  9

RC522_Driver reader(RC522_SS_PIN, RC522_RST_PIN);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  reader.begin();
  
  // Test communication and version detection
  uint8_t version = reader.getVersion();
  if (version == 0x90 || version == 0x91 || version == 0x92) {
    Serial.println("MFRC522 detected");
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
}
```

## API Reference

### Constructor
```cpp
RC522_Driver(uint8_t ssPin, uint8_t resetPin = 255, SPIClass &spi = SPI)
```
Initializes the RC522 reader with specified SS and reset pins.

### Configuration Methods
- `void begin()` - Initialize the RFID module  
- `void reset()` - Reset the RFID module
- `uint8_t getVersion()` - Get chip version number
- `bool selfTest()` - Run internal self-test
- `void setBaudRate(BaudRate rate)` - Set communication baud rate
- `void setModulation(bool ask100)` - Configure modulation type  
- `void setCRCMode(uint8_t preset)` - Set CRC mode

### RF Configuration 
- `void configureRF()` - Configure RF parameters for ISO 14443A
- `void printRFRegisters()` - Print all RF registers (debug)
- `void setAntenna(bool enabled)` - Enable/disable antenna
- `void setAntennaGain(PCD_RxGain gain)` - Set antenna gain level  
- `PCD_RxGain getAntennaGain()` - Get current antenna gain

### Communication Methods
- `StatusCode calculateCRC(const uint8_t* data, uint8_t length, uint8_t* result)` - Calculate CRC
- `StatusCode transceiveData(...)` - Transmit and receive data with the card

## Error Codes

- `STATUS_OK` - Operation completed successfully
- `STATUS_TIMEOUT` - Timeout occurred during operation  
- `STATUS_ERROR` - General error occurred
- `STATUS_COLLISION` - Bit collision detected
- `STATUS_NO_ROOM` - Buffer overflow or not enough space

## Supported Cards and Tags

- MIFARE Classic (1K, 4K)
- MIFARE Ultralight 
- ISO/IEC 14443A compatible cards
- Various RFID tags

## Troubleshooting

### Common Issues:
1. **No response from module**: Check wiring connections, especially SS and RST pins
2. **Wrong version returned (0x00 or 0xFF)**: Verify SPI connection and power supply  
3. **Timeout errors**: Ensure card is properly positioned on the antenna
4. **CRC errors**: Check for interference between multiple RFID readers

## Examples Included

1. `rf522_test.ino` - Basic test with version detection, self-test, and card reading
2. `rf522_Multimodule_with_shared_RST_test.ino` - Test for multi-module configurations

## Supported Platforms

- Arduino Uno/Mega/Nano
- ESP32 (VSPI classico)

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b feature/fooBar`)
3. Commit your changes (`git commit -am 'Add some fooBar'`) 
4. Push to the branch (`git push origin feature/fooBar`)
5. Create a new Pull Request

## License

This project is licensed under the MIT License.

## Author

Created by Nello Nardi