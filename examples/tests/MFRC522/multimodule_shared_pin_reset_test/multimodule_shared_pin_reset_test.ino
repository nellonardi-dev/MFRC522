
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

RC522_Driver* readers[NUM_READERS] = {};


// =====================================================
// RISULTATI DIAGNOSTICI
// =====================================================

struct ReaderDiagnostic
{
    uint8_t version = 0;
    bool initialized = false;
    bool selfTestPassed = false;
};

ReaderDiagnostic diagnostics[NUM_READERS];


// =====================================================
// STATO SCANSIONE
// =====================================================

unsigned long lastScan[NUM_READERS] = {};
unsigned long lastCard[NUM_READERS] = {};


// =====================================================
// IDENTIFICAZIONE VERSIONE
// =====================================================

const __FlashStringHelper* getVersionName(uint8_t version)
{
    switch (version)
    {
        case 0x88:
            return F("FM17522 Clone");

        case 0x90:
            return F("MFRC522 Version 0.0");

        case 0x91:
            return F("MFRC522 Version 1.0");

        case 0x92:
            return F("MFRC522 Version 2.0");

        case 0x00:
            return F("No response");

        case 0xFF:
            return F("Invalid response");

        default:
            return F("Unknown firmware");
    }
}


// =====================================================
// CREAZIONE DRIVER
// =====================================================

void createReaders()
{
    for (size_t i = 0; i < NUM_READERS; i++)
    {
        if (readers[i] != nullptr)
            continue;

        readers[i] = new RC522_Driver(SS_PINS[i]);

        if (readers[i] == nullptr && report)
        {
            Serial.print(F("Reader "));
            Serial.print(i + 1);
            Serial.println(F(": allocation failed!"));
        }
    }
}


// =====================================================
// HARDWARE RESET CONDIVISO
// =====================================================

void hardReset()
{
    if (report)
        Serial.println(F("\nPerforming shared hardware reset..."));

    // Deseleziona tutti i moduli SPI
    for (size_t i = 0; i < NUM_READERS; i++)
    {
        pinMode(SS_PINS[i], OUTPUT);
        digitalWrite(SS_PINS[i], HIGH);
    }

    // Reset hardware globale
    pinMode(RST_SHARED, OUTPUT);

    digitalWrite(RST_SHARED, LOW);
    delay(10);

    digitalWrite(RST_SHARED, HIGH);
    delay(50);

    if (report)
        Serial.println(F("Shared reset completed."));
}


// =====================================================
// START - INIZIALIZZAZIONE DI TUTTI I MODULI
// =====================================================

void start()
{
    if (report)
        Serial.println(F("\nStarting MFRC522 modules..."));

    for (size_t i = 0; i < NUM_READERS; i++)
    {
        // Invalida i risultati diagnostici precedenti
        diagnostics[i] = ReaderDiagnostic{};

        if (readers[i] == nullptr)
            continue;

        if (report)
        {
            Serial.print(F("Starting reader "));
            Serial.println(i + 1);
        }

        // Inizializzazione driver
        readers[i]->begin();

        diagnostics[i].initialized = true;

        if (report)
            Serial.println(F("Initialization completed."));
    }

    if (report)
        Serial.println(F("All modules initialization completed."));
}


// =====================================================
// RESTART - RESET HARDWARE + START
// =====================================================

void restart()
{
    if (report)
        Serial.println(F("\nRestarting all MFRC522 modules..."));

    hardReset();

    start();

    if (report)
        Serial.println(F("All modules restarted."));
}


// =====================================================
// TEST INDIVIDUALE MODULO
// =====================================================

void testReader(size_t index)
{
    if (index >= NUM_READERS || readers[index] == nullptr)
        return;

    ReaderDiagnostic& result = diagnostics[index];

    if (!result.initialized)
        return;

    if (report)
    {
        Serial.println();
        Serial.println(F("================================"));
        Serial.print(F("MFRC522 MODULE "));
        Serial.println(index + 1);
        Serial.println(F("================================"));
    }

    // -----------------------------------------------
    // 1. Lettura firmware
    // -----------------------------------------------

    result.version = readers[index]->getVersion();

    if (report)
    {
        Serial.print(F("Firmware: 0x"));

        if (result.version < 0x10)
            Serial.print('0');

        Serial.println(result.version, HEX);

        Serial.print(F("Chip: "));
        Serial.println(getVersionName(result.version));
    }

    // -----------------------------------------------
    // 2. Self-test digitale
    // -----------------------------------------------

    if (report)
        Serial.println(F("Running digital self-test..."));

    result.selfTestPassed = readers[index]->selfTest();

    if (report)
    {
        Serial.print(F("Self-test: "));
        Serial.println(
            result.selfTestPassed ? F("PASSED") : F("FAILED")
        );
    }
}


// =====================================================
// TEST MULTIMODULO
// =====================================================

void testAllReaders()
{
    if (report)
        Serial.println(F("\nStarting multimodule diagnostics..."));

    for (size_t i = 0; i < NUM_READERS; i++)
    {
        testReader(i);
    }
}


// =====================================================
// REPORT FINALE
// =====================================================

void printSummary()
{
    if (!report)
        return;

    uint8_t passed = 0;
    uint8_t failed = 0;
    uint8_t notInitialized = 0;

    Serial.println();
    Serial.println(F("================================"));
    Serial.println(F("       MULTIMODULE REPORT"));
    Serial.println(F("================================"));

    for (size_t i = 0; i < NUM_READERS; i++)
    {
        Serial.print(F("Reader "));
        Serial.print(i + 1);

        Serial.print(F(" | Version: 0x"));

        if (diagnostics[i].version < 0x10)
            Serial.print('0');

        Serial.print(diagnostics[i].version, HEX);

        Serial.print(F(" | "));

        if (readers[i] == nullptr || !diagnostics[i].initialized)
        {
            Serial.println(F("NOT INITIALIZED"));
            notInitialized++;
        }
        else if (diagnostics[i].selfTestPassed)
        {
            Serial.println(F("PASS"));
            passed++;
        }
        else
        {
            Serial.println(F("FAIL"));
            failed++;
        }
    }

    Serial.println(F("--------------------------------"));

    Serial.print(F("Modules passed: "));
    Serial.println(passed);

    Serial.print(F("Modules failed: "));
    Serial.println(failed);

    Serial.print(F("Not initialized: "));
    Serial.println(notInitialized);

    Serial.print(F("Total modules: "));
    Serial.println(NUM_READERS);

    Serial.println(F("================================"));
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(500);

    if (report)
    {
        Serial.println();
        Serial.println(F("MFRC522 MULTIMODULE TEST"));
        Serial.println(F("Shared hardware RESET"));
    }

    // 1. Creazione driver
    createReaders();

    // 2. Reset hardware e inizializzazione globale
    restart();

    // 3. Diagnostica
    testAllReaders();

    // 4. Report
    printSummary();

    if (report)
        Serial.println(F("\nDiagnostic procedure completed."));
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    // Diagnostica eseguita una sola volta all'avvio.
}
