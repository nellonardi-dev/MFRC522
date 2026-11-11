# MFRC522‑NTAG21x – Arduino Library for the MFRC522 RFID reader

## 📖 Overview
This library implements a **low‑level driver** and two high‑level protocols for the MFRC522 NFC/RFID chip.
| Layer | Files | Purpose |
|-------|-------|---------|
| Driver | `src/RC522_Driver.{cpp,h}` | SPI communication, register access, FIFO handling, reset & configuration helpers. |
| ISO/IEC 14443‑A | `src/RC522_ISO14443A.{cpp,h}` | Card discovery (REQ/WUP), anti‑collision, UID extraction, SAK handling and generic transceive wrapper. |
| NTAG21x | `src/RC522_NTAG21X.{cpp,h}` | High‑level helpers to read/write the 2 kB / 4 kB NTAG21x memory layout, factory reset and page dumps. |

The code is pure C++17 and depends only on Arduino core (`Arduino.h`, `SPI.h`). It compiles with any board that provides standard SPI.

### Design goals
- **Encapsulation** – All hardware access lives in `RC522_Driver`; protocol classes operate on a reference to this driver.  
- **Clear error handling** – Driver returns `StatusCode` enums; protocol layers translate them into `ISO14443A_Status`.  
- **Extensibility** – Adding other protocols (e.g., ISO/IEC 18092) follows the same pattern.

### Features
- Low‑level SPI driver with FIFO support and timeout handling.  
- Full ISO/IEC 14443‑A card discovery, anti‑collision, UID extraction and SAK handling.  
- NTAG21x (213/215/216) read/write, page dump and factory reset helpers.  
- CRC_A calculation according to ISO 14443‑A.  
- Example sketches for driver, ISO14443‑A test, NTAG21x auto‑dump and low‑level register dump.

### Installation
#### Arduino IDE
1. Open **Sketch → Include Library → Manage Libraries…**
2. Search for *MFRC522‑NTAG21x* and click **Install**.
3. Restart the IDE if prompted.
#### Manual (ZIP)
1. Download or clone this repository.
2. In Arduino IDE go to **Sketch → Include Library → Add .ZIP Library…** and select the downloaded ZIP.

### Hardware Connections
| Pin | MFRC522 | Arduino |
|-----|---------|--------|
| SCK | 5       | `SCK` |
| MOSI| 7       | `MOSI` |
| MISO| 6       | `MISO` |
| SS/SDA | 10   | any digital pin (example uses **5**) |
| RST | 9        | any digital pin (example uses **26** on ESP32) |

> ⚠️ The MFRC522 operates at **3.3 V**; if your board is 5 V use a level shifter or a 3.3 V module.

### Minimal Example
```cpp
#include <RC522_Driver.h>
#include <RC522_ISO14443A.h>
#define SS_PIN   5
#define RST_PIN 26
RC522_Driver driver(SS_PIN, RST_PIN);
RC522_ISO14443A reader(driver);
void setup(){
    Serial.begin(115200);
    if(!reader.begin()){Serial.println("MFRC522 init failed");while(true);}
}
void loop(){
    if(reader.request()==ISO14443A_OK && reader.anticollision()==ISO14443A_OK && reader.selectCard()==ISO14443A_OK){
        const ISO14443A_UID &uid=reader.getUID();
        Serial.print("UID: ");
        for(uint8_t i=0;i<uid.size;++i){if(uid.bytes[i]<0x10)Serial.print('0');Serial.print(uid.bytes[i],HEX);if(i<uid.size-1)Serial.print(':');}
        Serial.println();
        reader.halt();
    }
    delay(250);
}
```

### Examples Folder Structure
| Example | Description |
|---------|-------------|
| **RC522_ISO14443A_Test** | Demonstrates card detection and UID printing. |
| **NTAG21x_AutoDump** | Reads an NTAG213/215/216 tag and prints all memory pages in hex. |
| **iso_test** | Low‑level ISO14443-A register dump for debugging. |
| **rfid_module_test** | Full read/write test against a real RFID module (requires hardware). |

### Repository Layout
```
MFRC522-NTAG21x/
├── src/                     # Library source files (.cpp/.h)
│   ├── RC522_Driver.cpp
│   ├── RC522_Driver.h
│   ├── RC522_ISO14443A.cpp
│   ├── RC522_ISO14443A.h
│   ├── RC522_NTAG21X.cpp
│   └── RC522_NTAG21X.h
├── examples/                # Example sketches
│   ├── RC522_ISO14443A_Test/
│   │   └── RC522_ISO14443A_Test.ino
│   ├── NTAG21x_AutoDump/
│   │   └── NTAG21x_AutoDump.ino
│   ├── iso_test/
│   │   └── iso_test.ino
│   └── rfid_module_test/
│       └── rfid_module_test.ino
├── library.properties        # Arduino metadata
├── CHANGELOG.md              # Release notes
├── LICENSE                   # MIT license
└── README.md                 # This file
```

### Contributing
Feel free to open issues or pull requests on the GitHub repository.  Before submitting:
- Update `CHANGELOG.md` with your changes.
- Add or update tests if applicable (the library currently has no automated test suite).
- Ensure that the code compiles on at least one Arduino board in the IDE.

### License
MIT – see [LICENSE](LICENSE) for details.