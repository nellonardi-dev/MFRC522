
#include "RC522_Driver.h"

// ============================================================
// CONFIGURAZIONE
// ============================================================

constexpr bool report = true;

// ============================================================
// MAPPA PIN
// ============================================================
//
// MFRC522       Arduino UNO / Nano      Arduino Mega
// ---------------------------------------------------
// SDA / SS      D10                     D53
// SCK           D13                     D52
// MOSI          D11                     D51
// MISO          D12                     D50
// RST           D9                      D9
// 3.3V          3.3V                    3.3V
// GND           GND                     GND
//
// MFRC522       ESP32 (VSPI classico)
// -----------------------------------
// SDA / SS      GPIO 5
// SCK           GPIO 18
// MOSI          GPIO 23
// MISO          GPIO 19
// RST           GPIO 22
// 3.3V          3.3V
// GND           GND
//
// ATTENZIONE:
// Il MFRC522 deve essere alimentato a 3.3V.
//
// ============================================================
// CONFIGURAZIONE HARDWARE
// ============================================================

#if defined(ARDUINO_ARCH_ESP32)

constexpr uint8_t SS_PIN   = 5;
constexpr uint8_t RST_PIN  = 26;
constexpr uint8_t SCK_PIN  = 18;
constexpr uint8_t MISO_PIN = 19;
constexpr uint8_t MOSI_PIN = 23;

#else

#if defined(__AVR_ATmega2560__)
constexpr uint8_t SS_PIN = 53;
#else
constexpr uint8_t SS_PIN = 10;
#endif

constexpr uint8_t RST_PIN = 9;

#endif

const unsigned long cardPause = 2000;

RC522_Driver reader(SS_PIN, RST_PIN);


// ============================================================
// RISULTATI DEI TEST
// ============================================================

struct DiagnosticResults {
    uint8_t version;
    uint8_t txControl;
    uint8_t antennaGain;

    bool versionOK;
    bool antennaOK;
    bool selfTestOK;
};

DiagnosticResults results;


// ============================================================
// FUNZIONI DI SUPPORTO
// ============================================================

void printHex(uint8_t value) {
    if (value < 0x10) Serial.print('0');
    Serial.print(value, HEX);
}

void printResult(const char* name, bool result) {
    Serial.print("[");
    Serial.print(result ? " OK " : "FAIL");
    Serial.print("] ");
    Serial.println(name);
}


// ============================================================
// TEST 1 - VERSIONE CHIP
// ============================================================

bool testVersion() {

    results.version = reader.getVersion();

    return (
        results.version == 0x90 ||
        results.version == 0x91 ||
        results.version == 0x92 ||
        results.version == 0x88
    );
}


// ============================================================
// TEST 2 - ANTENNA
// ============================================================

bool testAntenna() {

    results.txControl = reader.peekRegister(TxControlReg);
    results.antennaGain = (uint8_t)reader.getAntennaGain();

    return (results.txControl & 0x03) != 0;
}


// ============================================================
// TEST 3 - SELF TEST
// ============================================================

bool testSelfTest() {

    return reader.selfTest();
}


// ============================================================
// REPORT DETTAGLIATO
// ============================================================

void printReport() {

    Serial.println();
    Serial.println("========================================");
    Serial.println("          RC522 - REPORT");
    Serial.println("========================================");

    // Versione
    Serial.println();
    Serial.println("[VERSIONE CHIP]");

    Serial.print("Valore registro VersionReg: 0x");
    printHex(results.version);
    Serial.println();

    switch (results.version) {
        case 0x90:
            Serial.println("Chip: MFRC522 versione 0.0");
            break;

        case 0x91:
            Serial.println("Chip: MFRC522 versione 1.0");
            break;

        case 0x92:
            Serial.println("Chip: MFRC522 versione 2.0");
            break;

        case 0x88:
            Serial.println("Chip: clone compatibile");
            break;

        default:
            Serial.println("Chip: versione sconosciuta");
            break;
    }

    // Antenna
    Serial.println();
    Serial.println("[ANTENNA]");

    Serial.print("TxControlReg: 0x");
    printHex(results.txControl);
    Serial.println();

    Serial.print("Stato antenna: ");
    Serial.println(results.antennaOK ? "ABILITATA" : "DISABILITATA");

    Serial.print("Guadagno antenna: ");
    Serial.println(results.antennaGain);

    // Self-test
    Serial.println();
    Serial.println("[SELF TEST]");

    Serial.println(results.selfTestOK
        ? "Test digitale superato"
        : "Test digitale fallito");

    // Registri RF
    Serial.println();
    Serial.println("[REGISTRI RF]");

    reader.printRFRegisters();

    Serial.println();
    Serial.println("========================================");
    Serial.println("          FINE REPORT");
    Serial.println("========================================");
}


// ============================================================
// ESECUZIONE DIAGNOSTICA
// ============================================================

void runDiagnostics() {

    Serial.println();
    Serial.println("========================================");
    Serial.println("        RC522 - DIAGNOSTICA");
    Serial.println("========================================");
    Serial.println();

    // Test 1
    results.versionOK = testVersion();
    printResult("Comunicazione / Versione chip", results.versionOK);

    // Test 2
    results.antennaOK = testAntenna();
    printResult("Antenna RF", results.antennaOK);

    // Test 3
    results.selfTestOK = testSelfTest();
    printResult("Self-test digitale", results.selfTestOK);

    // Riepilogo
    bool allOK =
        results.versionOK &&
        results.antennaOK &&
        results.selfTestOK;

    Serial.println();
    Serial.println("----------------------------------------");

    printResult("RISULTATO COMPLESSIVO", allOK);

    Serial.println("----------------------------------------");

    // Report dettagliato solo se abilitato
    if (report) {
        printReport();
    }

    Serial.println();
    Serial.println("In attesa di una carta...");
    Serial.println();
}


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

#if defined(ARDUINO_ARCH_ESP32)
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
#else
    SPI.begin();
#endif

    delay(500);

    reader.begin();

    runDiagnostics();
}


// ============================================================
// LOOP - RILEVAMENTO CARTE
// ============================================================

void loop() {
    static unsigned long lastDetection = 0;
    static unsigned long cardDetectedAt = 0;
    static bool cardPresent = false;

    const unsigned long scanInterval = 100;
    const unsigned long cardPause = 2000;

    // Dopo una lettura, sospende temporaneamente le richieste REQA
    if (cardPresent) {
        if (millis() - cardDetectedAt < cardPause) {
            return;
        }
        cardPresent = false;
    }

    if (millis() - lastDetection < scanInterval) {
        return;
    }
    lastDetection = millis();

    const uint8_t request = PICC_CMD_REQA;
    uint8_t response[8] = {0};
    uint8_t responseLength = 0;

    StatusCode status = reader.transceiveData(
        &request, 1,
        response, &responseLength, sizeof(response),
        7, 0, nullptr, nullptr, false
    );

    if (status == STATUS_OK && responseLength >= 2) {
        cardPresent = true;
        cardDetectedAt = millis();

        Serial.print("[CARD] Carta rilevata - ATQA: ");
        printHex(response[0]);
        Serial.print(" ");
        printHex(response[1]);
        Serial.println();
    }
}
