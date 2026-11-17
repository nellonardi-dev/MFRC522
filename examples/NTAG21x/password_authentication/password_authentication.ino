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

const uint8_t EXPECTED_PACK[2] = {
    0xAB, 0xCD
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

// -----------------------------------------------------
// Serial confirmation
// -----------------------------------------------------

bool waitForConfirmation() {

    Serial.println();
    Serial.println(F("[WARNING] AUTHLIM is different from zero."));
    Serial.println(F("[WARNING] Authentication attempts are limited."));
    Serial.println(F("Continue with password authentication?"));
    Serial.println(F("Type Y and press ENTER to continue."));

    while (true) {

        if (!Serial.available()) {
            delay(10);
            continue;
        }

        char c = Serial.read();

        if (c == '\r' || c == '\n')
            continue;

        delay(20);

        while (Serial.available())
            Serial.read();

        return c == 'Y' || c == 'y';
    }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);

    Serial.println();
    Serial.println(F("================================"));
    Serial.println(F(" NTAG21x PASSWORD AUTHENTICATION"));
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
    
/*
    // ---------------------------------
    // 4. Check password protection
    // ---------------------------------

    bool protectedTag =
        cfg.auth0 != NTAG21x_Cfg::AUTH0_DISABLED &&
        cfg.auth0 <= info.lastPage;

    if (!protectedTag) {

        Serial.println(F("[ERROR] Tag is not password protected."));

        finishCardOperation(originalUID);
        return;
    }

    Serial.print(F("[CHECK] Password protection active from page "));
    Serial.println(cfg.auth0);

*/    
    // ---------------------------------
    // 5. Check AUTHLIM
    // ---------------------------------

    Serial.print(F("[CHECK] AUTHLIM: "));
    Serial.println(cfg.authLimit);

    if (cfg.authLimit != 0) {

        if (!waitForConfirmation()) {

            Serial.println(F("[AUTH] Operation cancelled by user."));

            finishCardOperation(originalUID);
            return;
        }
    }
    // ---------------------------------
    // 6. Password authentication
    // ---------------------------------

    Serial.println(F("[AUTH] Executing PWD_AUTH..."));

    uint8_t receivedPACK[2] = {0, 0};

    status = ntag.authenticate(
        PASSWORD,
        receivedPACK
    );

    if (status != NTAG21X_OK) {

        Serial.println(F("[AUTH] AUTHENTICATION FAILED"));

        Serial.print(F("Status: "));
        Serial.println(ntag21xStatusText(status));

        if (status == NTAG21X_AUTH_LOCKED) {
            Serial.println(F("[WARNING] AUTHLIM reached."));
        }

        finishCardOperation(originalUID);
        return;
    }

    Serial.println(F("[AUTH] AUTHENTICATION SUCCESSFUL"));

    // ---------------------------------
    // 7. Verify PACK
    // ---------------------------------

    Serial.print(F("Received PACK: "));

    for (uint8_t i = 0; i < 2; i++) {

        if (receivedPACK[i] < 0x10)
            Serial.print('0');

        Serial.print(receivedPACK[i], HEX);
        Serial.print(' ');
    }

    Serial.println();

    if (
        receivedPACK[0] == EXPECTED_PACK[0] &&
        receivedPACK[1] == EXPECTED_PACK[1]
    ) {

        Serial.println(F("[PACK] VERIFICATION SUCCESSFUL"));

    } else {

        Serial.println(F("[PACK] VERIFICATION FAILED"));
    }

    finishCardOperation(originalUID);
}



