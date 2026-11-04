
#include <SPI.h>
#include "RC522_Driver.h"

// =====================================================
// CONFIGURAZIONE
// =====================================================

constexpr bool report = true;

// Pin RST condiviso da tutti i moduli
constexpr uint8_t RST_SHARED = 9;

// Pin SS individuali
constexpr uint8_t SS_PINS[] = {
    5,
    16
};

// Numero di moduli
constexpr size_t NUM_READERS =
    sizeof(SS_PINS) / sizeof(SS_PINS[0]);

// Intervallo minimo tra due richieste dello stesso lettore
constexpr unsigned long SCAN_INTERVAL = 100;

// Pausa dopo una risposta valida
constexpr unsigned long CARD_PAUSE = 2000;


// =====================================================
// ARRAY LETTORI MFRC522
// =====================================================

RC522_Driver* readers[NUM_READERS] = {
    nullptr
};

// Stato indipendente per ogni lettore
unsigned long lastScan[NUM_READERS] = {0};
bool cardPauseActive[NUM_READERS] = {false};
unsigned long cardDetectedAt[NUM_READERS] = {0};


// =====================================================
// FUNZIONI DI SUPPORTO
// =====================================================

void printHex(uint8_t value) {
    if (value < 0x10) Serial.print("0");
    Serial.print(value, HEX);
}

void printResult(const char* name, bool result) {
    Serial.print("[TEST] ");
    Serial.print(name);
    Serial.print(" : ");
    Serial.println(result ? "OK" : "ERRORE");
}


// =====================================================
// RESET HARDWARE CONDIVISO
// =====================================================

void hardResetShared() {

    pinMode(RST_SHARED, OUTPUT);

    digitalWrite(RST_SHARED, LOW);
    delay(10);

    digitalWrite(RST_SHARED, HIGH);
    delay(50);

    
}


// =====================================================
// TEST DIAGNOSTICO SINGOLO LETTORE
// =====================================================

void testReader(size_t index) {

    RC522_Driver& reader = *readers[index];

    Serial.println();
    Serial.println("--------------------------------");

    Serial.print("TEST READER ");
    Serial.println(index + 1);

    Serial.println("--------------------------------");

    // Test versione
    uint8_t version = reader.getVersion();

    bool versionOK =
        version != 0x00 && version != 0xFF;

    printResult("Versione MFRC522", versionOK);

    // Test antenna
    uint8_t txControl =
        reader.peekRegister(TxControlReg);

    bool antennaOK =
        (txControl & 0x03) == 0x03;

    printResult("Antenna attiva", antennaOK);

    // Test digitale
    bool selfTestOK = reader.selfTest();

    printResult("Self Test digitale", selfTestOK);

    if (report) {

        Serial.print("VersionReg: 0x");
        printHex(version);
        Serial.println();

        Serial.print("TxControlReg: 0x");
        printHex(reader.peekRegister(TxControlReg));
        Serial.println();

        Serial.print("Antenna Gain: ");
        Serial.println(
            static_cast<uint8_t>(reader.getAntennaGain()),
            HEX
        );

        Serial.println("Registri RF:");
        reader.printRFRegisters();
    }
}


// =====================================================
// LETTURA CARTA SINGOLO LETTORE
// =====================================================

void scanReader(size_t index) {

    unsigned long now = millis();

    // Gestione pausa indipendente
    if (cardPauseActive[index]) {

        if (now - cardDetectedAt[index] < CARD_PAUSE) {
            return;
        }

        cardPauseActive[index] = false;
    }

    // Intervallo tra richieste REQA
    if (now - lastScan[index] < SCAN_INTERVAL) {
        return;
    }

    lastScan[index] = now;

    // Richiesta REQA
    const uint8_t request = PICC_CMD_REQA;

    uint8_t response[8] = {0};
    uint8_t responseLength = 0;

    StatusCode status = readers[index]->transceiveData(
        &request,
        1,
        response,
        &responseLength,
        sizeof(response),
        7,       // REQA: frame di 7 bit
        0,
        nullptr,
        nullptr,
        false
    );

    // Verifica risposta ATQA
    if (status == STATUS_OK && responseLength >= 2) {

        cardPauseActive[index] = true;
        cardDetectedAt[index] = millis();

        Serial.print("[CARD] READER ");
        Serial.print(index + 1);

        Serial.print(" ha rilevato una carta - ATQA: ");

        printHex(response[0]);
        Serial.print(" ");
        printHex(response[1]);

        Serial.println();
    }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println("TEST MULTIMODULO MFRC522");
    Serial.println("RESET HARDWARE CONDIVISO");
    Serial.println("================================");

    // Preparazione pin SS
    for (size_t i = 0; i < NUM_READERS; i++) {

        pinMode(SS_PINS[i], OUTPUT);
        digitalWrite(SS_PINS[i], HIGH);
    }

    // Costruzione lettori senza RST
    for (size_t i = 0; i < NUM_READERS; i++) {

        readers[i] = new RC522_Driver(SS_PINS[i]);
    }

    // Reset hardware comune
    hardResetShared();
    Serial.println("[RESET] Reset hardware condiviso completato");

    // Inizializzazione sequenziale
    for (size_t i = 0; i < NUM_READERS; i++) {

        Serial.print("[INIT] Inizializzazione READER ");
        Serial.println(i + 1);

        readers[i]->begin();
    }

    Serial.println();
    Serial.println("[INIT] Tutti i lettori inizializzati");

    // Test diagnostici
    for (size_t i = 0; i < NUM_READERS; i++) {

        testReader(i);
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("SCANSIONE CARTE ATTIVA");
    Serial.println("================================");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

    // Scansione sequenziale di tutti i lettori
    for (size_t i = 0; i < NUM_READERS; i++) {

        scanReader(i);
    }
}
