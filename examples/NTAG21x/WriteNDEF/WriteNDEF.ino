#include "RC522_Driver.h"
#include "RC522_ISO14443A.h"
#include "RC522_NTAG21X.h"

#include <string.h>

// ============================================================
// HARDWARE CONFIGURATION
// ============================================================

#define SS_PIN  5
#define RST_PIN 26

RC522_Driver mfrc522_driver(SS_PIN, RST_PIN);
RC522_ISO14443A iso14443a(mfrc522_driver);
RC522_NTAG21x ntag21x(iso14443a);

// ============================================================
// NDEF CONFIGURATION
// ============================================================

// Example: NFC Forum Well-Known Text record.
// The record type is an ASCII string, so its length is calculated
// by createTextRecord() with strlen().
const char* TEXT_MESSAGE  = "HELLO WORLD!";
const char* TEXT_LANGUAGE = "en";

// This example emits UTF-8 text.
constexpr bool TEXT_UTF8 = true;

// NTAG215 user memory is 504 bytes. Keep a larger working buffer
// if you later want to test other tag models or larger records.
constexpr uint16_t NDEF_BUFFER_SIZE = 900;

uint8_t recordPayload[NDEF_BUFFER_SIZE];
uint8_t rawData[NDEF_BUFFER_SIZE];
uint16_t DATA_LENGTH = 0;

// ============================================================
// NTAG WRITE CONFIGURATION
// ============================================================

constexpr uint8_t START_PAGE = 0x04;
constexpr uint8_t PAGE_OFFSET = 0; // 0..3

// ============================================================
// GENERIC NDEF RECORD DESCRIPTION
// ============================================================

// A record describes the NDEF fields, not their serialized bytes.
//
// type is an ASCII NDEF type string (for example "T", "U",
// "text/plain", or "application/example").
// typeLength is derived with strlen() by the helper functions.
//
// payload is a byte buffer. payloadLength is calculated by
// createTextRecord(); for arbitrary binary records it must be
// supplied because binary data has no terminator.

struct NDEF_Record {
    uint8_t tnf;
    const char* type;
    uint8_t typeLength;
    const uint8_t* payload;
    uint32_t payloadLength;
};

// ============================================================
// TEXT RECORD FACTORY
// ============================================================

// Creates the payload of an NFC Forum Text Record:
//
// Status byte | Language code | Text
//
// Status bit 7: 0 = UTF-8, 1 = UTF-16
// Status bits 0..5: language-code length
//
// This helper calculates all string lengths automatically.
// The payload is written into payloadBuffer, whose lifetime must
// extend until encodeNdefRecord() has finished.

bool createTextRecord(
    const char* text,
    const char* language,
    uint8_t* payloadBuffer,
    uint16_t payloadCapacity,
    NDEF_Record& record,
    bool utf8 = true
) {
    if (!text || !language || !payloadBuffer) {
        return false;
    }

    const size_t textLength = strlen(text);
    const size_t languageLength = strlen(language);

    if (languageLength == 0 || languageLength > 63) {
        return false;
    }

    const uint32_t payloadLength =
        1UL + (uint32_t)languageLength + (uint32_t)textLength;

    if (payloadLength > payloadCapacity) {
        return false;
    }

    // NDEF Text Record: TNF Well-Known, Type "T".
    static const char TEXT_TYPE[] = "T";

    uint16_t pos = 0;

    // Status byte: encoding flag + language-code length.
    payloadBuffer[pos++] =
        (utf8 ? 0x00 : 0x80) | (languageLength & 0x3F);

    memcpy(payloadBuffer + pos, language, languageLength);
    pos += languageLength;

    memcpy(payloadBuffer + pos, text, textLength);
    pos += textLength;

    record.tnf = 0x01; // TNF_WELL_KNOWN
    record.type = TEXT_TYPE;
    record.typeLength = strlen(TEXT_TYPE);
    record.payload = payloadBuffer;
    record.payloadLength = pos;

    return true;
}

// ============================================================
// GENERIC NDEF ENCODER
// ============================================================

// Serializes one NDEF record into a Type 2 Tag NDEF Message TLV.
//
// Automatically calculates:
// - NDEF record size
// - Short Record (SR) or normal payload-length field
// - TLV short or extended length
// - Total output size
//
// Returns the number of bytes written, or 0 on error.
//
// Note: this helper encodes one record only. MB and ME are therefore
// both set. The type is an ASCII string; arbitrary binary Type fields
// can be supported by extending NDEF_Record with an explicit type
// byte length and a uint8_t* type pointer.

uint16_t encodeNdefRecord(
    const NDEF_Record& record,
    uint8_t* output,
    uint16_t outputCapacity
) {
    if (!output || !record.type ||
        (!record.payload && record.payloadLength > 0) ||
        record.tnf > 0x07) {
        return 0;
    }

    const size_t calculatedTypeLength = strlen(record.type);

    if (calculatedTypeLength > 255 ||
        calculatedTypeLength != record.typeLength) {
        return 0;
    }

    const uint8_t typeLength = (uint8_t)calculatedTypeLength;
    const bool shortRecord = record.payloadLength <= 255;

    // NDEF record:
    // Header(1) + Type Length(1) + Payload Length(1 or 4)
    // + Type + Payload.
    const uint32_t ndefLength =
        2UL + (shortRecord ? 1UL : 4UL) +
        typeLength + record.payloadLength;

    if (ndefLength > 65535UL) {
        return 0;
    }

    // Type 2 Tag TLV uses one length byte below 0xFF;
    // otherwise 0xFF followed by a 16-bit length.
    const bool extendedTlv = ndefLength >= 255;

    const uint32_t totalLength =
        1UL + (extendedTlv ? 3UL : 1UL) +
        ndefLength + 1UL; // TLV terminator

    if (totalLength > outputCapacity) {
        return 0;
    }

    uint16_t pos = 0;

    // --------------------------------------------------------
    // TYPE 2 TAG TLV
    // --------------------------------------------------------

    output[pos++] = 0x03; // NDEF Message TLV

    if (extendedTlv) {
        output[pos++] = 0xFF;
        output[pos++] = (ndefLength >> 8) & 0xFF;
        output[pos++] = ndefLength & 0xFF;
    } else {
        output[pos++] = (uint8_t)ndefLength;
    }

    // --------------------------------------------------------
    // NDEF RECORD HEADER
    // --------------------------------------------------------

    // MB=1, ME=1, CF=0, SR=automatic, IL=0, TNF=record.tnf
    output[pos++] =
        0xC0 | (shortRecord ? 0x10 : 0x00) | (record.tnf & 0x07);

    output[pos++] = typeLength;

    if (shortRecord) {
        output[pos++] = (uint8_t)record.payloadLength;
    } else {
        output[pos++] = (record.payloadLength >> 24) & 0xFF;
        output[pos++] = (record.payloadLength >> 16) & 0xFF;
        output[pos++] = (record.payloadLength >> 8) & 0xFF;
        output[pos++] = record.payloadLength & 0xFF;
    }

    // --------------------------------------------------------
    // TYPE + PAYLOAD
    // --------------------------------------------------------

    memcpy(output + pos, record.type, typeLength);
    pos += typeLength;

    if (record.payloadLength > 0) {
        memcpy(output + pos, record.payload, record.payloadLength);
        pos += record.payloadLength;
    }

    // Terminator TLV.
    output[pos++] = 0xFE;

    return pos;
}

// ============================================================
// DEBUG
// ============================================================

void printHex(const uint8_t* data, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        if (data[i] < 0x10) {
            Serial.print('0');
        }
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

// ============================================================
// WAIT FOR CARD REMOVAL
// ============================================================

void waitForCardRemoval() {
    Serial.print(F("[WAIT] Remove the tag from the reader..."));

    uint8_t consecutiveMisses = 0;
    constexpr uint8_t REQUIRED_MISSES = 3;

    while (consecutiveMisses < REQUIRED_MISSES) {
        ISO14443A_Status status = iso14443a.wakeup();

        if (status == ISO14443A_TIMEOUT ||
            status == ISO14443A_NO_CARD) {
            consecutiveMisses++;
            delay(300);
            continue;
        }

        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        status = iso14443a.selectCard();

        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        consecutiveMisses = 0;
        iso14443a.halt();
        delay(300);
    }

    Serial.println(F(" done."));
    delay(500);
}

// ============================================================
// RAW MEMORY WRITE + VERIFY
// ============================================================

bool writeRawMemory() {
    const NTAG21x_Info& info = ntag21x.getInfo();

    Serial.println();
    Serial.println(F("=== NDEF MEMORY WRITE ==="));

    if (PAGE_OFFSET > 3) {
        Serial.println(F("[ERROR] Invalid page offset (0-3)"));
        return false;
    }

    const uint16_t writeOffset =
        ((uint16_t)START_PAGE - 0x04) * 4 + PAGE_OFFSET;

    Serial.print(F("Start page: 0x"));
    Serial.println(START_PAGE, HEX);

    Serial.print(F("Page offset: "));
    Serial.println(PAGE_OFFSET);

    Serial.print(F("Absolute offset: "));
    Serial.println(writeOffset);

    Serial.print(F("User memory: "));
    Serial.print(info.userBytes);
    Serial.println(F(" bytes"));

    Serial.print(F("Encoded TLV size: "));
    Serial.println(DATA_LENGTH);

    if (START_PAGE < 0x04 ||
        writeOffset >= info.userBytes ||
        DATA_LENGTH > info.userBytes - writeOffset) {
        Serial.println(F("[ERROR] Write exceeds user memory"));
        return false;
    }

    Serial.println(F("Data to write:"));
    printHex(rawData, DATA_LENGTH);

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

    uint8_t readBuffer[NDEF_BUFFER_SIZE] = { 0 };

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

    Serial.println(F("Read-back data:"));
    printHex(readBuffer, DATA_LENGTH);

    if (memcmp(readBuffer, rawData, DATA_LENGTH) != 0) {
        Serial.println(F("[VERIFY ERROR] Data mismatch"));
        return false;
    }

    Serial.println(F("[SUCCESS] Data verification passed"));
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
    Serial.println(F("          Write NDEF"));
    Serial.println(F("================================"));

    // Create the Text record. String lengths are calculated here.
    NDEF_Record record;

    if (!createTextRecord(
            TEXT_MESSAGE,
            TEXT_LANGUAGE,
            recordPayload,
            sizeof(recordPayload),
            record,
            TEXT_UTF8)) {
        Serial.println(F("[ERROR] Text record creation failed"));
        while (true) {
            delay(100);
        }
    }

    // Serialize the generic NDEF record and Type 2 Tag TLV.
    DATA_LENGTH = encodeNdefRecord(
        record,
        rawData,
        sizeof(rawData)
    );

    if (DATA_LENGTH == 0) {
        Serial.println(F("[ERROR] NDEF encoding failed"));
        while (true) {
            delay(100);
        }
    }

    Serial.print(F("[NDEF] Text: "));
    Serial.println(TEXT_MESSAGE);

    Serial.print(F("[NDEF] Language: "));
    Serial.println(TEXT_LANGUAGE);

    Serial.print(F("[NDEF] Encoded size: "));
    Serial.print(DATA_LENGTH);
    Serial.println(F(" bytes"));

    Serial.println(F("[NDEF] Encoded data:"));
    printHex(rawData, DATA_LENGTH);

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

    const ISO14443A_UID& uid = iso14443a.getUID();

    Serial.println();
    Serial.println(F("[EVENT] Card detected"));

    Serial.print(F("UID: "));
    for (uint8_t i = 0; i < uid.size; i++) {
        if (uid.bytes[i] < 0x10) {
            Serial.print('0');
        }
        Serial.print(uid.bytes[i], HEX);
        Serial.print(' ');
    }
    Serial.println();

    NTAG21x_Status status = ntag21x.begin();

    if (status != NTAG21X_OK) {
        Serial.print(F("[ERROR] "));
        Serial.println(ntag21xStatusText(status));

        iso14443a.halt();
        waitForCardRemoval();
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
            waitForCardRemoval();
            return;
    }

    writeRawMemory();
    
    iso14443a.halt();

    waitForCardRemoval();
}