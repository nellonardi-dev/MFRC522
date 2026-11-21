#include "RC522_Driver.h"
#include "RC522_ISO14443A.h"
#include "RC522_NTAG21X.h"

// ============================================================
// CONFIGURAZIONE HARDWARE
// ============================================================

#define SS_PIN  5
#define RST_PIN 26

// ============================================================
// ISTANZE
// ============================================================

RC522_Driver mfrc522_driver(SS_PIN, RST_PIN);
RC522_ISO14443A iso14443a(mfrc522_driver);
RC522_NTAG21x ntag21x(iso14443a);

// ============================================================
// DATI DA SCRIVERE
// ============================================================

// Buffer di esempio: 
const uint8_t rawData[] = {

    // NDEF TLV
    0x03,                   // TLV Type: NDEF Message
    0x1C,                   // TLV Length: 28 bytes

    // NDEF Record Header
    0xD1,                   // MB=1, ME=1, SR=1, TNF=Well Known
    0x01,                   // Type Length
    0x18,                   // Payload Length: 24 bytes
    0x54,                   // Type: Text

    // Text Record Payload
    0x02,                   // UTF-8, language length = 2
    0x69, 0x74,              // Language: "it"

    // Text: "Ciao dal mondo ESP32!"
    0x43, 0x69, 0x61, 0x6F,
    0x20,
    0x64, 0x61, 0x6C,
    0x20,
    0x6D, 0x6F, 0x6E, 0x64, 0x6F,
    0x20,
    0x45, 0x53, 0x50, 0x33, 0x32, 0x21,

    // TLV Terminator
    0xFE
};


constexpr uint16_t DATA_LENGTH = sizeof(rawData);

// ============================================================
// CONFIGURAZIONE SCRITTURA
// ============================================================

// Pagina NTAG21x di partenza
constexpr uint8_t START_PAGE = 0x04;

// Offset del byte iniziale nella pagina (0-3)
constexpr uint8_t PAGE_OFFSET = 0;
// Offset relativo alla memoria utente.


// ============================================================
// FUNZIONI DI SUPPORTO
// ============================================================

void printHex(const uint8_t* data, uint16_t length) {

    for (uint16_t i = 0; i < length; i++) {

        if (data[i] < 0x10)
            Serial.print('0');

        Serial.print(data[i], HEX);
        Serial.print(' ');
    }

    Serial.println();
}

// ============================================================
// ATTESA RIMOZIONE CARTA
// ============================================================

void waitForCardRemoval(const ISO14443A_UID& originalUID) {

    Serial.print(F("[ATTESA] Rimuovi la carta dal lettore..."));

    uint8_t consecutiveMisses = 0;
    constexpr uint8_t REQUIRED_MISSES = 3;

    while (consecutiveMisses < REQUIRED_MISSES) {

        // WUPA: risveglia le carte in HALT
        ISO14443A_Status status = iso14443a.wakeup();

        if (status == ISO14443A_TIMEOUT ||
            status == ISO14443A_NO_CARD) {

            consecutiveMisses++;
            delay(300);
            continue;
        }

        // Errori diversi da timeout: non considerarli rimozione
        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        // Una carta ha risposto: esegui SELECT
        status = iso14443a.selectCard();

        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        // Recupera UID della carta rilevata
        const ISO14443A_UID& detectedUID = iso14443a.getUID();

        bool sameCard =
            detectedUID.size == originalUID.size &&
            memcmp(detectedUID.bytes,
                   originalUID.bytes,
                   originalUID.size) == 0;

        if (sameCard) {
            // La carta originale è ancora presente
            consecutiveMisses = 0;
        } else {
            // È stata rilevata una carta con UID differente
            consecutiveMisses = 0;
        }

        // Riporta in HALT la carta selezionata
        iso14443a.halt();

        delay(300);
    }

    Serial.println(F(" fatto."));
    delay(500);
}

// ============================================================
// SCRITTURA MEMORIA RAW
// ============================================================

bool writeRawMemory() {

    const NTAG21x_Info& info = ntag21x.getInfo();

    Serial.println();
    Serial.println(F("=== RAW MEMORY WRITE ==="));

    // Verifica offset pagina
    if (PAGE_OFFSET > 3) {
        Serial.println(F("[ERROR] Invalid page offset (0-3)"));
        return false;
    }

    // Calcolo offset assoluto nella memoria utente
    const uint16_t writeOffset =
        ((uint16_t)START_PAGE - 0x04) * 4 + PAGE_OFFSET;

    Serial.print(F("Start page: 0x"));
    Serial.println(START_PAGE, HEX);

    Serial.print(F("Page offset: "));
    Serial.println(PAGE_OFFSET);

    Serial.print(F("Absolute offset: "));
    Serial.println(writeOffset);

    Serial.print(F("Memory size: "));
    Serial.print(info.userBytes);
    Serial.println(F(" bytes"));

    Serial.print(F("Data length: "));
    Serial.println(DATA_LENGTH);

    // Controllo preventivo dei limiti
    if (START_PAGE < 0x04 ||
        writeOffset >= info.userBytes ||
        DATA_LENGTH > info.userBytes - writeOffset) {

        Serial.println(F("[ERROR] Write exceeds user memory"));
        return false;
    }

    Serial.println();
    Serial.println(F("Data to write:"));
    printHex(rawData, DATA_LENGTH);

    // Scrittura sequenziale dei byte
    NTAG21x_Status status = ntag21x.writeBytes(
        writeOffset,
        rawData,
        DATA_LENGTH,
        true
    );

    if (status != NTAG21X_OK) {

        Serial.print(F("[WRITE ERROR] "));
        Serial.println(ntag21xStatusText(status));

        return false;
    }

    Serial.println(F("[OK] Writing completed"));

    // Verifica lettura
    uint8_t readBuffer[DATA_LENGTH] = {0};

    status = ntag21x.readBytes(
        writeOffset,
        readBuffer,
        DATA_LENGTH
    );

    if (status != NTAG21X_OK) {

        Serial.print(F("[READ ERROR] "));
        Serial.println(ntag21xStatusText(status));

        return false;
    }

    Serial.println();
    Serial.println(F("Read back data:"));
    printHex(readBuffer, DATA_LENGTH);

    // Confronto byte per byte
    if (memcmp(readBuffer, rawData, DATA_LENGTH) != 0) {

        Serial.println(F("[VERIFY ERROR] Data mismatch!"));
        return false;
    }

    Serial.println();
    Serial.println(F("[SUCCESS] Data verification passed!"));

    return true;
}


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    while (!Serial) {
        ;
    }

    Serial.println();
    Serial.println(F("================================"));
    Serial.println(F("     NTAG21x Raw Memory Write"));
    Serial.println(F("================================"));

    SPI.begin();

    if (!iso14443a.begin()) {

        Serial.println(F("[ERROR] MFRC522 initialization failed"));

        while (true) {
            delay(100);
        }
    }

    Serial.println(F("[OK] MFRC522 initialized"));
    Serial.println(F("Place an NTAG215 tag on the reader"));
}

// ============================================================
// LOOP
// ============================================================

void loop() {

    if (iso14443a.readCardSerial() != ISO14443A_OK) {
        delay(100);
        return;
    }

    // Conserva l'UID della carta originale
    const ISO14443A_UID& uid = iso14443a.getUID();

    Serial.println();
    Serial.println(F("[EVENT] Card detected"));

    Serial.print(F("UID: "));

    for (uint8_t i = 0; i < uid.size; i++) {

        if (uid.bytes[i] < 0x10)
            Serial.print('0');

        Serial.print(uid.bytes[i], HEX);
        Serial.print(' ');
    }

    Serial.println();

    // Inizializzazione NTAG21x
    NTAG21x_Status status = ntag21x.begin();

    if (status != NTAG21X_OK) {

        Serial.print(F("[ERROR] "));
        Serial.println(ntag21xStatusText(status));

        iso14443a.halt();
        waitForCardRemoval(uid);

        return;
    }

    const NTAG21x_Info& info = ntag21x.getInfo();

    Serial.print(F("Detected model: "));

    switch (info.model) {

        case NTAG213:
            Serial.println(F("NTAG213"));
            break;

        case NTAG215:
            Serial.println(F("NTAG215"));
            break;

        case NTAG216:
            Serial.println(F("NTAG216"));
            break;

        default:
            Serial.println(F("Unknown"));

            iso14443a.halt();
            waitForCardRemoval(uid);

            return;
    }

    // Esegue la scrittura esclusivamente su NTAG215
    if (info.model == NTAG215) {

        writeRawMemory();

    } else {

        Serial.println(F("[ERROR] This example requires NTAG215"));
    }

    // Termina la comunicazione con il tag
    iso14443a.halt();

    // Attende la rimozione fisica della carta
    waitForCardRemoval(uid);
}

