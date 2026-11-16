# MFRC522-NTAG21x

**A modular Arduino library for the MFRC522 RFID/NFC reader, ISO/IEC 14443‑A communication and NTAG21x tags.**
The implementation closely follows the official NXP data sheets:
* MFRC522 – <https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf>
* NTAG213/215/216 – <https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf>
Copies of these PDF documents are included in the project under the `datasheet` directory for quick reference.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## Overview

MFRC522-NTAG21x is an Arduino library designed to provide a modular interface to the MFRC522 RFID/NFC reader.

The library is organized into three independent but interconnected layers:

- **Driver layer:** handles low-level communication with the MFRC522 through SPI.
- **Protocol layer:** implements ISO/IEC 14443-A card discovery and communication.
- **Tag layer:** provides high-level operations for NTAG21x tags.

This architecture separates hardware access from protocol implementation and tag-specific operations, making the library easier to maintain, extend and debug.

## Features

- Low-level MFRC522 driver with SPI communication.
- Register access and FIFO management.
- Hardware reset and reader configuration helpers.
- Timeout handling and status-code-based error reporting.
- ISO/IEC 14443-A card discovery.
- REQA and WUPA commands.
- Anti-collision and UID extraction.
- SAK handling and card selection.
- CRC_A calculation.
- NTAG213, NTAG215 and NTAG216 memory operations.
- NTAG21x page reading and writing.
- Memory dump utilities.
- Factory reset helpers.
- Example sketches for testing and development.

## Architecture

The library is divided into three main layers.

| Layer | Main files | Responsibility |
|---|---|---|
| Driver | `RC522_Driver.h/.cpp` | SPI communication, registers, FIFO, reset and hardware configuration |
| ISO/IEC 14443-A | `RC522_ISO14443A.h/.cpp` | Card discovery, anti-collision, UID extraction and card selection |
| NTAG21x | `RC522_NTAG21X.h/.cpp` | NTAG213/215/216 memory operations |

### 1. RC522 Driver

`RC522_Driver` is the lowest-level component of the library.

It provides direct access to the MFRC522 hardware and handles:

- SPI communication.
- Register read and write operations.
- FIFO management.
- Hardware reset.
- Reader configuration.
- CRC-related operations.
- Error and status reporting.

The driver is designed to be independent of higher-level RFID protocols.

### 2. ISO/IEC 14443-A

`RC522_ISO14443A` implements the communication procedures required to discover and select ISO/IEC 14443-A cards.

Its main responsibilities include:

- REQA and WUPA requests.
- Card detection.
- Anti-collision procedures.
- UID extraction.
- SAK handling.
- Card selection.
- Generic transceive operations.

This layer uses an existing `RC522_Driver` instance rather than accessing the hardware directly.

### 3. NTAG21x

`RC522_NTAG21X` provides higher-level operations for NTAG21x tags.

Supported tag families include:

- NTAG213
- NTAG215
- NTAG216

The layer provides utilities for working with tag memory, including page reading, page writing, memory dumps and factory-reset operations.

## Installation

### Arduino IDE

1. Download or clone this repository.
2. In Arduino IDE, select **Sketch → Include Library → Add .ZIP Library...**
3. Select the downloaded repository ZIP file.
4. Include the required library headers in your sketch.

### Manual installation

Copy the library folder into your Arduino libraries directory.

The folder should contain the library metadata file and the source files.

Restart Arduino IDE after installation if necessary.

## Hardware connections

The MFRC522 communicates with the microcontroller through SPI.

| MFRC522 pin | Microcontroller connection |
|---|---|
| SDA / SS | Any suitable digital output pin |
| SCK | SPI SCK |
| MOSI | SPI MOSI |
| MISO | SPI MISO |
| RST | Any suitable digital output pin |
| GND | GND |
| 3.3V | 3.3V supply |

**Important:** The MFRC522 operates at 3.3 V. Do not connect its power supply or signal pins directly to 5 V unless the module and the electrical interface are explicitly designed to support it.

SPI pin assignments depend on the selected microcontroller board. Always check the board documentation.

## Quick start

The following example initializes the reader, detects an ISO/IEC 14443-A card and prints its UID.

```cpp
#include <RC522_Driver.h>
#include <RC522_ISO14443A.h>

#define SS_PIN   5
#define RST_PIN  26

RC522_Driver driver(SS_PIN, RST_PIN);
RC522_ISO14443A reader(driver);

void setup() {
    Serial.begin(115200);

    if (!reader.begin()) {
        Serial.println("MFRC522 initialization failed");
        while (true);
    }

    Serial.println("MFRC522 initialized");
}

void loop() {
    if (reader.request() == ISO14443A_OK &&
        reader.anticollision() == ISO14443A_OK &&
        reader.selectCard() == ISO14443A_OK) {

        const ISO14443A_UID &uid = reader.getUID();

        Serial.print("UID: ");

        for (uint8_t i = 0; i < uid.size; i++) {
            if (uid.bytes[i] < 0x10) {
                Serial.print('0');
            }

            Serial.print(uid.bytes[i], HEX);

            if (i < uid.size - 1) {
                Serial.print(':');
            }
        }

        Serial.println();

        reader.halt();
    }

    delay(250);
}
```

This example uses the ISO/IEC 14443-A layer. The driver is instantiated separately and passed to the protocol object.

## Multiple MFRC522 readers

The library can also be used in projects with multiple MFRC522 modules connected to the same SPI bus.

A typical configuration uses:

- Shared SCK, MOSI and MISO lines.
- A separate SS pin for each reader.
- Optionally, a shared RST line.

Each reader must have its own driver and protocol instances.

When multiple modules share the same reset line, resetting that line affects every connected reader. All readers must therefore be initialized again after a shared hardware reset.

For a complete example, including `hardReset()`, `start()` and `restart()` functions, see the dedicated multi-reader guide in the project documentation.

## Examples

The `examples/` directory contains a variety of sketches that showcase the library's functionality. The folder hierarchy has been reorganised compared to earlier releases; the table below lists the current examples with their paths relative to the repository root.

| Example (path) | Description |
|---|---|
| `examples/tests/ISO14443A/card_test` | Card detection, selection and UID reading. |
| `examples/tests/ISO14443A/request_test` | Low‑level ISO/IEC 14443‑A request tests (REQA/WUPA). |
| `examples/NTAG21x/NTAG21x_AutoDump` | Reads the full memory of an NTAG21x tag and prints its contents. |
| `examples/NTAG21x/NTAG21x_Factory_Reset` | Performs a factory reset on an NTAG21x tag. |
| `examples/tests/MFRC522/single_module_test` | Basic single‑module test: version detection, self‑test and card read. |
| `examples/tests/MFRC522/multimodule_shared_pin_reset_test` | Demonstrates handling of multiple MFRC522 modules with a shared reset pin. |

Check the individual example folders for the corresponding sketches and implementation details.

## Error handling

The library uses status codes to report the outcome of hardware and protocol operations.

The driver layer provides its own status information, while higher-level protocol classes translate relevant errors into protocol-specific status codes.

Always check the return value of operations that can fail, especially during card discovery, communication and memory access.

## Design principles

The library follows a few core design principles:

- **Separation of concerns:** hardware access, protocol handling and tag operations are kept in separate layers.
- **Encapsulation:** protocol classes use the driver interface instead of directly manipulating hardware registers.
- **Explicit error handling:** operations report their status through dedicated codes.
- **Extensibility:** additional protocols and tag families can be implemented as separate layers.

## Contributing

Contributions, bug reports and suggestions are welcome.

Before submitting changes:

1. Check the existing code structure and conventions.
2. Update the `CHANGELOG.md` when appropriate.
3. Add or update examples when introducing new functionality.
4. Test changes on real hardware whenever possible.
5. Open an issue or pull request describing the changes.

See [CONTRIBUTING.md](CONTRIBUTING.md) for additional information.

## License

This project is distributed under the MIT License.

See the [LICENSE](LICENSE) file for details.