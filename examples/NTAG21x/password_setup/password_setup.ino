#include <RC522_Driver.h>
#include <RC522_ISO14443A.h>
#include <RC522_NTAG21X.h>
#include <string.h>

// =====================================================
// HARDWARE CONFIGURATION
// =====================================================

#define SS_PIN   5
#define RST_PIN  26

// =====================================================
// PASSWORD CONFIGURATION
// =====================================================

const uint8_t PASSWORD[4] = {
    0x12, 0x34, 0x56, 0x78
};

const uint8_t PACK[2] = {
    0xAB, 0xCD
};

const uint8_t AUTH0 = 4;

// =====================================================
// OPTIONAL VALIDATION
// =====================================================

struct ValidationOptions {
    bool checkNDEF;
};

const ValidationOptions VALIDATION = {
    false
};

// =====================================================
// OBJECTS
// =====================================================

RC522_Driver driver(SS_PIN, RST_PIN);
RC522_ISO14443A reader(driver);
RC522_NTAG21x ntag(reader);

// =====================================================
// HELPERS
// =====================================================

const char* getModelName(NTAG21x_Model model) {

    switch (model) {
        case NTAG213: return "NTAG213";
        case NTAG215: return "NTAG215";
        case NTAG216: return "NTAG216";
        default:      return "UNKNOWN";
    }
}

// -----------------------------------------------------
// Print UID
// -----------------------------------------------------

void printUID(const ISO14443A_UID& uid) {

    Serial.print(F("UID: "));

    for (uint8_t i = 0; i < uid.size; i++) {

        if (uid.bytes[i] < 0x10)
            Serial.print('0');

        Serial.print(uid.bytes[i], HEX);

        if (i < uid.size - 1)
            Serial.print(':');
    }

    Serial.println();
}

// -----------------------------------------------------
// Optional NDEF check
// -----------------------------------------------------

bool checkNDEF(bool& found) {

    found = false;

    const NTAG21x_Info& info = ntag.getInfo();

    uint16_t offset = 0;

    while (offset < info.userBytes) {

        uint8_t type;

        if (ntag.readBytes(offset, &type, 1) != NTAG21X_OK)
            return false;

        offset++;

        // NULL TLV
        if (type == 0x00)
            continue;

        // Terminator TLV
        if (type == 0xFE)
            break;

        uint8_t length;

        if (ntag.readBytes(offset, &length, 1) != NTAG21X_OK)
            return false;

        offset++;

        uint16_t dataLength = length;

        // Extended length
        if (length == 0xFF) {

            uint8_t extendedLength[2];

            if (offset + 2 > info.userBytes)
                return false;

            if (ntag.readBytes(offset, extendedLength, 2) != NTAG21X_OK)
                return false;

            offset += 2;

            dataLength =
                ((uint16_t)extendedLength[0] << 8) |
                extendedLength[1];
        }

        // NDEF Message TLV
        if (type == 0x03) {
            found = true;
            return true;
        }

        if (offset > info.userBytes ||
            dataLength > info.userBytes - offset)
            return false;

        offset += dataLength;
    }

    return true;
}

// -----------------------------------------------------
// Wait for card removal
// -----------------------------------------------------

void waitForCardRemoval(const ISO14443A_UID& originalUID) {

    Serial.print(F("[WAIT] Remove card from reader..."));

    uint8_t consecutiveMisses = 0;
    constexpr uint8_t REQUIRED_MISSES = 3;

    while (consecutiveMisses < REQUIRED_MISSES) {

        // WUPA: risveglia le carte in HALT
        ISO14443A_Status status = reader.wakeup();

        if (status == ISO14443A_TIMEOUT ||
            status == ISO14443A_NO_CARD) {

            consecutiveMisses++;
            delay(300);
            continue;
        }

        // Errori diversi da timeout
        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        // Anticollisione e SELECT
        status = reader.selectCard();

        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        const ISO14443A_UID& detectedUID = reader.getUID();

        bool sameCard =
            detectedUID.size == originalUID.size &&
            memcmp(
                detectedUID.bytes,
                originalUID.bytes,
                originalUID.size
            ) == 0;

        // Qualunque carta presente significa che
        // la rimozione non è ancora stata completata.
        if (sameCard) {
            consecutiveMisses = 0;
        } else {
            consecutiveMisses = 0;
        }

        reader.halt();

        delay(300);
    }

    Serial.println(F(" DONE."));
    delay(500);
}

// -----------------------------------------------------
// End card operation
// -----------------------------------------------------

void finishCardOperation(const ISO14443A_UID& uid) {

    reader.halt();
    waitForCardRemoval(uid);
}

// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);

    Serial.println();
    Serial.println(F("================================"));
    Serial.println(F(" NTAG21x PASSWORD SETUP"));
    Serial.println(F("================================"));

    if (!reader.begin()) {

        Serial.println(F("ERROR: MFRC522 initialization failed"));

        while (true);
    }

    Serial.println(F("MFRC522 initialized."));
    Serial.println(F("Waiting for NTAG21x..."));
}

// =====================================================
// LOOP
// =====================================================

void loop() {

    // ---------------------------------
    // 1. Detect and select card
    // ---------------------------------

    if (reader.request() != ISO14443A_OK)
        return;

    if (reader.selectCard() != ISO14443A_OK)
        return;

    // Keep original UID for removal detection
    ISO14443A_UID originalUID = reader.getUID();

    Serial.println();
    Serial.println(F("[CARD] Tag detected"));

    printUID(originalUID);

    // ---------------------------------
    // 2. Detect NTAG21x model
    // ---------------------------------

    NTAG21x_Status status = ntag.begin();

    if (status != NTAG21X_OK) {

        Serial.print(F("[ERROR] NTAG detection failed: "));
        Serial.println(ntag21xStatusText(status));

        finishCardOperation(originalUID);
        return;
    }

    const NTAG21x_Info& info = ntag.getInfo();

    Serial.print(F("Model: "));
    Serial.println(getModelName(info.model));

    // ---------------------------------
    // 3. Read configuration
    // ---------------------------------

    NTAG21x_Config cfg;

    status = ntag.readConfig(cfg);

    if (status != NTAG21X_OK) {

        Serial.print(F("[ERROR] Configuration read failed: "));
        Serial.println(ntag21xStatusText(status));

        finishCardOperation(originalUID);
        return;
    }

    // ---------------------------------
    // 4. Check existing protection
    // ---------------------------------

    bool protectedTag =
        cfg.auth0 != NTAG21x_Cfg::AUTH0_DISABLED &&
        cfg.auth0 <= info.lastPage;

    if (protectedTag) {

        Serial.println(F("[ERROR] Tag already password protected."));
        Serial.print(F("AUTH0: "));
        Serial.println(cfg.auth0);

        finishCardOperation(originalUID);
        return;
    }

    Serial.println(F("[CHECK] Password protection: NOT ACTIVE"));

    // ---------------------------------
    // 5. Optional NDEF validation
    // ---------------------------------

    if (VALIDATION.checkNDEF) {

        bool hasNDEF = false;

        if (!checkNDEF(hasNDEF)) {

            Serial.println(F("[ERROR] NDEF validation failed."));

            finishCardOperation(originalUID);
            return;
        }

        if (hasNDEF) {

            Serial.println(F("[ERROR] NDEF message already present."));

            finishCardOperation(originalUID);
            return;
        }

        Serial.println(F("[CHECK] NDEF validation passed."));
    }

    // ---------------------------------
    // 6. Enable password protection
    // ---------------------------------

    Serial.println(F("[SETUP] Configuring password protection..."));

    status = ntag.enablePasswordProtection(
        PASSWORD,
        PACK,
        AUTH0,
        false,  // PROT: read access remains free
        0       // AUTHLIM: unlimited attempts
    );

    if (status == NTAG21X_OK) {

        Serial.println();
        Serial.println(F("================================"));
        Serial.println(F(" PASSWORD SETUP SUCCESSFUL"));
        Serial.println(F("================================"));

    } else {

        Serial.println();
        Serial.println(F("[ERROR] PASSWORD SETUP FAILED"));

        Serial.print(F("Status: "));
        Serial.println(ntag21xStatusText(status));
    }

    finishCardOperation(originalUID);
}
