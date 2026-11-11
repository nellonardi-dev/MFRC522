#include "RC522_Driver.h"

// =====================================================
// CONFIGURAZIONE HARDWARE
// =====================================================

#define RC522_SS_PIN   5
#define RC522_RST_PIN  26

RC522_Driver rc522(RC522_SS_PIN, RC522_RST_PIN);

// =====================================================
// COSTANTI CONFIGURAZIONE RF
// =====================================================

constexpr uint8_t TEST_TX_MODE  = 0x00;
constexpr uint8_t TEST_RX_MODE  = 0x00;
constexpr uint8_t TEST_TX_ASK   = 0x40;
constexpr uint8_t TEST_MODWIDTH = 0x26;
constexpr uint8_t TEST_MODE_REG = 0x3D;

// =====================================================
// FUNZIONI DI SUPPORTO
// =====================================================

void printResult(const char* description, bool result) {

    Serial.print(description);
    Serial.print(" : ");

    Serial.println(result ? "OK" : "ERRORE");
}

void printHex(const char* description, uint8_t value) {

    Serial.print(description);
    Serial.print(" : 0x");

    if (value < 0x10) {
        Serial.print("0");
    }

    Serial.println(value, HEX);
}

// =====================================================
// TEST VERSIONE CHIP
// =====================================================

bool testVersion() {

    Serial.println("\n--- VERSIONE CHIP ---");

    uint8_t version = rc522.getVersion();

    printHex("VersionReg", version);

    switch (version) {

        case 0x91:
            Serial.println("MFRC522 Version 1.0");
            return true;

        case 0x92:
            Serial.println("MFRC522 Version 2.0");
            return true;

        case 0x88:
            Serial.println("Clone FM17522 o compatibile");
            return true;

        case 0x90:
            Serial.println("Versione non identificata");
            return false;

        case 0x00:
            Serial.println("Nessuna comunicazione SPI");
            return false;

        case 0xFF:
            Serial.println("Bus SPI non funzionante");
            return false;

        default:
            Serial.println("Chip non riconosciuto");
            return false;
    }
}

// =====================================================
// TEST SELF TEST
// =====================================================

bool testSelfTest() {

    Serial.println("\n--- SELF TEST ---");

    bool result = rc522.selfTest();

    printResult("Self-test interno", result);

    return result;
}

// =====================================================
// TEST CONFIGURAZIONE RF
// =====================================================

void testRFConfiguration() {

    Serial.println("\n--- CONFIGURAZIONE RF ---");

    rc522.configureRF(
        TEST_TX_MODE,
        TEST_RX_MODE,
        TEST_TX_ASK,
        TEST_MODWIDTH,
        TEST_MODE_REG
    );

    rc522.setBaudRate(RC522_Driver::BAUD_106KBPS);

    rc522.setModulation(true);

    Serial.println("Configurazione RF applicata");

    rc522.printRFRegisters();
}

// =====================================================
// TEST GAIN ANTENNA
// =====================================================

void testAntennaGain() {

    Serial.println("\n--- ANTENNA GAIN ---");

    rc522.setAntennaGain(RxGain_avg);

    PCD_RxGain gain = rc522.getAntennaGain();

    printHex("Gain register", static_cast<uint8_t>(gain));
}

// =====================================================
// TEST ANTENNA
// =====================================================

void testAntenna() {

    Serial.println("\n--- ANTENNA ---");

    rc522.setAntenna(true);

    Serial.println("Antenna RF attivata");
}

// =====================================================
// TEST CRC HARDWARE
// =====================================================

bool testCRC() {

    Serial.println("\n--- CRC HARDWARE ---");

    const uint8_t testData[] = {
        0x12,
        0x34,
        0x56
    };

    uint8_t crc[2] = {0};

    StatusCode status = rc522.calculateCRC(
        testData,
        sizeof(testData),
        crc
    );

    if (status != STATUS_OK) {

        Serial.println("Calcolo CRC fallito");

        return false;
    }

    Serial.print("CRC_A: ");

    if (crc[0] < 0x10) Serial.print("0");

    Serial.print(crc[0], HEX);

    Serial.print(" ");

    if (crc[1] < 0x10) Serial.print("0");

    Serial.println(crc[1], HEX);

    return true;
}
// =====================================================
// TEST CRC software
// =====================================================

void calculateCRC_A_Software(
    const uint8_t* data,
    uint8_t length,
    uint8_t* result
) {
    uint16_t crc = 0x6363;

    for (uint8_t i = 0; i < length; i++) {

        uint8_t value = data[i] ^ (uint8_t)(crc & 0x00FF);

        value ^= value << 4;

        crc = (crc >> 8) ^
              ((uint16_t)value << 8) ^
              ((uint16_t)value << 3) ^
              ((uint16_t)value >> 4);
    }

    result[0] = crc & 0xFF;
    result[1] = (crc >> 8) & 0xFF;
}
// =====================================================
// TEST CRC COMPARATIVO H/S
// =====================================================

bool testCRCComparison() {

    Serial.println("\n--- CRC HARDWARE / SOFTWARE ---");

    const uint8_t testData[] = {
        0x12, 0x34, 0x56
    };

    uint8_t crcHardware[2] = {0};
    uint8_t crcSoftware[2] = {0};

    // Calcolo hardware
    StatusCode status = rc522.calculateCRC(
        testData,
        sizeof(testData),
        crcHardware
    );

    if (status != STATUS_OK) {
        Serial.println("Errore calcolo CRC hardware");
        return false;
    }

    // Calcolo software
    calculateCRC_A_Software(
        testData,
        sizeof(testData),
        crcSoftware
    );

    // Visualizzazione risultati
    Serial.print("CRC Hardware: ");
    Serial.print(crcHardware[0], HEX);
    Serial.print(" ");
    Serial.println(crcHardware[1], HEX);

    Serial.print("CRC Software: ");
    Serial.print(crcSoftware[0], HEX);
    Serial.print(" ");
    Serial.println(crcSoftware[1], HEX);

    // Confronto
    bool match =
        crcHardware[0] == crcSoftware[0] &&
        crcHardware[1] == crcSoftware[1];

    printResult("Confronto CRC", match);

    return match;
}

// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("       RC522 DRIVER TEST");
    Serial.println("================================");

    // Inizializzazione hardware
    rc522.begin();

    Serial.println("\nDriver inizializzato");

    // Versione chip
    bool versionOK = testVersion();

    if (!versionOK) {

        Serial.println("\nERRORE: verificare collegamenti SPI");

        return;
    }

    // Self-test
    testSelfTest();

    // Configurazione RF
    testRFConfiguration();

    // Gain antenna
    testAntennaGain();

    // Attivazione antenna
    testAntenna();

    // CRC hardware/software
    bool crcOK = testCRCComparison();

    Serial.println("\n================================");
    Serial.println("       TEST COMPLETATO");
    Serial.println("================================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

}