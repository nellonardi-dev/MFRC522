#include "RC522_Driver.h"
#include "RC522_ISO14443A.h"

//================================================
// HARDWARE SETUP
//================================================

#define SS_PIN  5
#define RST_PIN 26

RC522_Driver driver(SS_PIN, RST_PIN);
RC522_ISO14443A iso(driver);

void printHex(uint8_t value) {
    if (value < 0x10) {
        Serial.print('0');
    }
    Serial.print(value, HEX);
}
void printChipInfo() {

    Serial.println();
    Serial.println("================================");
    Serial.println("       RC522 CHIP INFO");
    Serial.println("================================");

    // Versione chip
    uint8_t version = driver.getVersion();

    Serial.print("VersionReg       : 0x");
    if (version < 0x10) Serial.print('0');
    Serial.println(version, HEX);

    if (version == 0x91) {
        Serial.println("Chip             : MFRC522 v1.0");
    }
    else if (version == 0x92) {
        Serial.println("Chip             : MFRC522 v2.0");
    }
    else {
        Serial.println("Chip             : Versione sconosciuta");
    }

    // Stato libreria ISO
    Serial.print("ISO initialized  : ");
    Serial.println(iso.isInitialized() ? "YES" : "NO");

    // Configurazione RF
    Serial.println();
    
    driver.printRFRegisters();

 // Guadagno antenna
    Serial.println();
    Serial.println("--- ANTENNA ---");

    PCD_RxGain gain = driver.getAntennaGain();

    Serial.print("Antenna gain reg : 0x");
    Serial.println((uint8_t)gain, HEX);

    uint8_t txControl = driver.peekRegister(TxControlReg);

    Serial.print("Antenna TX       : ");
    Serial.println((txControl & 0x03) ? "ON" : "OFF");
  }

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println();
    Serial.println("================================");
    Serial.println("    RC522 ISO14443A TEST");
    Serial.println("================================");

    if (!iso.begin()) {
        Serial.println("Errore inizializzazione ISO14443A");
        while (true) {}
    }
    printChipInfo();
    Serial.println("Libreria ISO14443A inizializzata");
    Serial.println("Avvicina una carta al lettore...");
}

void loop() {

    ISO14443A_Status status = iso.request();

    if (status == ISO14443A_OK) {

        const uint8_t* atqa = iso.getATQA();

        Serial.print("Carta presente - ATQA: ");

        if (atqa[0] < 0x10) Serial.print('0');
        Serial.print(atqa[0], HEX);

        Serial.print(' ');

        if (atqa[1] < 0x10) Serial.print('0');
        Serial.println(atqa[1], HEX);
    }

    delay(2000);
}

