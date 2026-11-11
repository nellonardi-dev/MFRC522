#ifndef RC522_NTAG21X_H
#define RC522_NTAG21X_H

#include <Arduino.h>
#include "RC522_ISO14443A.h"

namespace NTAG21x_Command {
    constexpr uint8_t READ          = 0x30;
    constexpr uint8_t WRITE         = 0xA2;
    constexpr uint8_t GET_VERSION   = 0x60;
    constexpr uint8_t FAST_READ     = 0x3A;
    constexpr uint8_t PWD_AUTH      = 0x1B;
    constexpr uint8_t COMP_WRITE    = 0xA0;
    constexpr uint8_t READ_CNT      = 0x39;
    constexpr uint8_t READ_SIG      = 0x3C;
}

namespace NTAG21x_Response {
    constexpr uint8_t ACK = 0x0A;
    // Datasheet tab. 23. Per "accesso negato" il datasheet non indica un
    // codice dedicato: in pratica si osserva 0x0.
    constexpr uint8_t NAK_INVALID_ARGUMENT = 0x00;
    constexpr uint8_t NAK_CRC_PARITY       = 0x01;
    constexpr uint8_t NAK_AUTH_LIMIT       = 0x04;  // contatore tentativi PWD esaurito
    constexpr uint8_t NAK_EEPROM_WRITE     = 0x05;  // errore di scrittura EEPROM
    constexpr uint8_t NAK_MASK = 0x0F;
}

namespace NTAG21x_Protocol {
    constexpr uint8_t PAGE_SIZE = 4;
    constexpr uint8_t READ_PAGE_COUNT = 4;
    constexpr uint8_t READ_DATA_SIZE = 16;
    constexpr uint8_t FAST_READ_MAX_PAGES = 15;
    constexpr uint8_t CRC_SIZE = 2;
    constexpr uint8_t MAX_PAGE_COUNT = 231;
    constexpr uint8_t MAX_FIFO_SIZE = 64;
    constexpr uint8_t SIGNATURE_SIZE = 32;
    constexpr uint8_t SIGNATURE_ADDR = 0x00;
    constexpr uint8_t COUNTER_ADDR = 0x02;
    constexpr uint8_t COUNTER_SIZE = 3;
}

namespace NTAG21x_Version {
    constexpr uint8_t VENDOR_NXP = 0x04;
    constexpr uint8_t PRODUCT_TYPE = 0x04;
    constexpr uint8_t PRODUCT_SUBTYPE = 0x02;
    constexpr uint8_t MAJOR_VERSION = 0x01;
    constexpr uint8_t NTAG213_SIZE_CODE = 0x0F;
    constexpr uint8_t NTAG215_SIZE_CODE = 0x11;
    constexpr uint8_t NTAG216_SIZE_CODE = 0x13;
}

enum NTAG21x_NAKStatus : uint8_t {
    NAK_NONE = 0x00,
    NAK_INVALID_ARGUMENT = 0x01,    // argomento non valido / accesso negato
    NAK_CRC_PARITY_ERROR = 0x02,
    NAK_AUTH_LIMIT_REACHED = 0x03,  // AUTHLIM esaurito
    NAK_EEPROM_WRITE_ERROR = 0x04,
    NAK_UNKNOWN = 0xFF
};

namespace NTAG21x_Memory {
    constexpr uint8_t PAGE_USER_START = 0x04;

    // Pagine finali NON utilizzabili come memoria utente (datasheet 8.5):
    // Dynamic Lock, CFG0, CFG1, PWD, PACK = 5 pagine.
    // (con 6 si perdeva l'ultima pagina utente: 213 -> 140 byte invece di 144)
    constexpr uint8_t RESERVED_PAGES = 5;

    // Ultima pagina di memoria per modello
    constexpr uint8_t NTAG213_LAST_PAGE = 0x2C;
    constexpr uint8_t NTAG215_LAST_PAGE = 0x86;
    constexpr uint8_t NTAG216_LAST_PAGE = 0xE6;

    // Parametri comuni
    constexpr uint8_t PAGE_SIZE = 4;
}

enum NTAG21x_Model : uint8_t {
    NTAG21X_UNKNOWN = 0,
    NTAG213,
    NTAG215,
    NTAG216
};

enum NTAG21x_Status : uint8_t {
    NTAG21X_OK = 0,
    NTAG21X_NOT_INITIALIZED,
    NTAG21X_UNSUPPORTED,
    NTAG21X_TIMEOUT,
    NTAG21X_NAK,
    NTAG21X_CRC_ERROR,
    NTAG21X_PROTOCOL_ERROR,
    NTAG21X_INVALID_ARGUMENT,
    NTAG21X_BUFFER_TOO_SMALL,
    NTAG21X_PARTIAL_DUMP,
    NTAG21X_AUTH_FAILED,
    NTAG21X_VERIFY_FAILED,   // scrittura ACK-ata ma rilettura diversa
    NTAG21X_LOCKED,          // CFGLCK attivo: configurazione bloccata
    NTAG21X_AUTH_LOCKED,     // AUTHLIM esaurito: PWD_AUTH non più possibile
    NTAG21X_MIRROR_CONFLICT, // scrittura parziale su pagina coperta dal mirror
    NTAG21X_ERROR
};

struct NTAG21x_Info {
    NTAG21x_Model model;
    uint8_t version[8];
    uint16_t totalBytes;
    uint16_t userBytes;
    uint8_t lastPage;
    uint8_t firstUserPage;
    uint8_t lastUserPage;
    uint8_t dynamicLockPage;
    uint8_t configStartPage;   // CFG0 (CFG1 = +1)
    uint8_t pwdPage;           // PWD  (sola scrittura)
    uint8_t packPage;          // PACK (sola scrittura)
};

struct NTAG21x_Dump {
    uint8_t memory[NTAG21x_Protocol::MAX_PAGE_COUNT][NTAG21x_Protocol::PAGE_SIZE];
    bool pageValid[NTAG21x_Protocol::MAX_PAGE_COUNT];
    uint8_t lastPage;
    bool complete;
};

// ---------------------------------------------------------------------------
// Pagine di configurazione (datasheet 8.5.7 - 8.5.9)
//   CFG0: [MIRROR][RFUI][MIRROR_PAGE][AUTH0]
//   CFG1: [ACCESS][VCTID][RFUI][RFUI]
// ---------------------------------------------------------------------------
namespace NTAG21x_Cfg {
    // Indici di pagina relativi a configStartPage
    constexpr uint8_t PAGE_CFG0 = 0;
    constexpr uint8_t PAGE_CFG1 = 1;

    // Byte MIRROR (CFG0 byte 0)
    constexpr uint8_t MIRROR_CONF_SHIFT = 6;     // bit 7:6
    constexpr uint8_t MIRROR_BYTE_SHIFT = 4;     // bit 5:4
    constexpr uint8_t MIRROR_FIELDS_MASK = 0xF0;
    constexpr uint8_t STRG_MOD_EN       = 0x04;  // bit 2

    // Byte ACCESS (CFG1 byte 0)
    constexpr uint8_t ACCESS_PROT             = 0x80; // 1 = anche lettura protetta
    constexpr uint8_t ACCESS_CFGLCK           = 0x40; // 1 = config bloccata (IRREVERSIBILE)
    constexpr uint8_t ACCESS_NFC_CNT_EN       = 0x10;
    constexpr uint8_t ACCESS_NFC_CNT_PWD_PROT = 0x08;
    constexpr uint8_t ACCESS_AUTHLIM_MASK     = 0x07;

    constexpr uint8_t AUTH0_DISABLED = 0xFF;

    // Lunghezza in byte (ASCII) dei mirror
    constexpr uint8_t MIRROR_LEN_UID     = 14;
    constexpr uint8_t MIRROR_LEN_COUNTER = 6;
    constexpr uint8_t MIRROR_LEN_BOTH    = 21;   // UID + 'x' + counter
}

enum NTAG21x_MirrorMode : uint8_t {
    NTAG21X_MIRROR_OFF         = 0,
    NTAG21X_MIRROR_UID         = 1,
    NTAG21X_MIRROR_COUNTER     = 2,
    NTAG21X_MIRROR_UID_COUNTER = 3
};

// Configurazione globale del chip, decodificata.
struct NTAG21x_Config {
    bool lockValid;            // staticLock / capabilityContainer letti
    bool configValid;          // dynamicLock / cfg0 / cfg1 letti (falso se protetti da PROT)

    uint8_t staticLock[2];          // pagina 2, byte 2-3
    uint8_t capabilityContainer[4]; // pagina 3
    uint8_t dynamicLock[3];         // pagina dynamicLockPage, byte 0-2
    uint8_t cfg0[4];                // grezzi
    uint8_t cfg1[4];                // grezzi

    // CFG0
    NTAG21x_MirrorMode mirrorMode;
    uint8_t mirrorByte;        // 0..3
    uint8_t mirrorPage;
    uint8_t auth0;             // prima pagina protetta (0xFF = nessuna)
    bool strongModulation;     // STRG_MOD_EN

    // CFG1
    bool readProtected;        // PROT
    bool configLocked;         // CFGLCK
    bool nfcCounterEnabled;    // NFC_CNT_EN
    bool nfcCounterPwdProtected; // NFC_CNT_PWD_PROT
    uint8_t authLimit;         // 0 = illimitati
    uint8_t vctid;             // CFG1 byte 1: RFUI nel datasheet, 0x05 sui tag reali

    // Static lock bits: il bit n di staticLock[0] blocca la pagina n (3..7),
    // il bit n di staticLock[1] blocca la pagina n+8 (8..15).
    bool isStaticPageLocked(uint8_t page) const {
        if (page < 3 || page > 15) return false;
        return page < 8 ? ((staticLock[0] >> page) & 1)
                        : ((staticLock[1] >> (page - 8)) & 1);
    }
};

class RC522_NTAG21x {
public:
    explicit RC522_NTAG21x(RC522_ISO14443A& iso);
    NTAG21x_Status begin();

    NTAG21x_Status readPage(uint8_t page, uint8_t data[NTAG21x_Protocol::PAGE_SIZE]);
    NTAG21x_Status fastRead(uint8_t startPage, uint8_t endPage,
                            uint8_t* data, uint16_t capacity,
                            uint16_t* bytesRead = nullptr);
    NTAG21x_Status readDump(NTAG21x_Dump& dump);

    const NTAG21x_Info& getInfo() const;
    bool isDetected() const;
    NTAG21x_NAKStatus getLastNAKStatus() const;

    // ---------------- Scrittura e autenticazione ----------------
    NTAG21x_Status writePage(uint8_t page, const uint8_t data[NTAG21x_Protocol::PAGE_SIZE]);
    NTAG21x_Status authenticate(const uint8_t password[4], uint8_t pack[2] = nullptr);

    // ---------------- Configurazione globale ----------------
    NTAG21x_Status readConfig(NTAG21x_Config& cfg);
    void printConfig(Print& out, const NTAG21x_Config& cfg) const;

    // Legge la configurazione e ne stampa l'interpretazione in italiano.
    NTAG21x_Status interpretConfig(Print& out);
    void interpretConfig(Print& out, const NTAG21x_Config& cfg) const;

    NTAG21x_Status setStrongModulation(bool enable);
    NTAG21x_Status setMirror(NTAG21x_MirrorMode mode, uint8_t page = 0, uint8_t byteOffset = 0);
    NTAG21x_Status setAuth0(uint8_t firstProtectedPage);          // 0xFF = disattiva
    NTAG21x_Status setReadProtection(bool enable);                // PROT
    NTAG21x_Status setNfcCounter(bool enable);                    // NFC_CNT_EN
    NTAG21x_Status setNfcCounterPasswordProtection(bool enable);  // NFC_CNT_PWD_PROT
    NTAG21x_Status setAuthLimit(uint8_t limit);                   // 0..7, 0 = illimitati
    NTAG21x_Status setPassword(const uint8_t password[4]);
    NTAG21x_Status setPack(const uint8_t pack[2]);
    NTAG21x_Status lockConfig(bool confirm);                      // IRREVERSIBILE

    // ---------------- Accesso a byte (memoria utente) ----------------
    // offset 0 = pagina 4 byte 0. Letture e scritture oltre userBytes: errore.
    NTAG21x_Status readBytes(uint16_t offset, uint8_t* data, uint16_t length);
    NTAG21x_Status writeBytes(uint16_t offset, const uint8_t* data, uint16_t length,
                              bool verify = true);

    // ---------------- Contatore NFC e firma ----------------
    NTAG21x_Status readCounter(uint32_t& value);
    NTAG21x_Status readSignature(uint8_t signature[NTAG21x_Protocol::SIGNATURE_SIZE]);

    // ---------------- Password: procedura completa ----------------
    NTAG21x_Status enablePasswordProtection(const uint8_t password[4], const uint8_t pack[2],
                                            uint8_t firstProtectedPage,
                                            bool protectRead = false,
                                            uint8_t authLimit = 0);
    NTAG21x_Status disablePasswordProtection();

    // ---------------- Lock (TUTTI IRREVERSIBILI) ----------------
    bool isPageLocked(uint8_t page, const NTAG21x_Config& cfg) const;
    NTAG21x_Status lockPage(uint8_t page, bool confirm);
    NTAG21x_Status freezeLockBits(uint8_t page, bool confirm);
    NTAG21x_Status makeReadOnly(bool confirm);

private:
    NTAG21x_Status commandWithData(const uint8_t* request, uint8_t requestLen,
                                   uint8_t* data, uint8_t dataLen);
    NTAG21x_Status getMirrorSpan(bool& active, uint8_t& firstPage, uint8_t& lastPage);
    bool lockBitFor(uint8_t page, bool block, uint8_t& byteIndex, uint8_t& mask,
                    bool& dynamic) const;
    NTAG21x_Status setLockBit(uint8_t page, bool block, bool confirm);
    NTAG21x_Status orLockBytes(bool dynamic, const uint8_t masks[3]);
    NTAG21x_Status checkAck(const uint8_t* response, uint8_t recvLen, uint8_t rxLastBits);
    NTAG21x_Status checkConfigUnlocked();
    NTAG21x_Status modifyConfigPage(uint8_t pageOffset, const uint8_t mask[4],
                                    const uint8_t value[4]);
    NTAG21x_Status modifyConfigByte(uint8_t pageOffset, uint8_t byteIndex,
                                    uint8_t mask, uint8_t value);
    RC522_ISO14443A* _iso;
    NTAG21x_Info _info;
    bool _initialized;
    NTAG21x_Status getVersion();
    NTAG21x_Status appendCRC(const uint8_t* data, uint8_t length, uint8_t* output);
    NTAG21x_Status mapISOStatus(ISO14443A_Status status) const;
    NTAG21x_Status readRangeRaw(uint8_t startPage, uint8_t endPage,
                                uint8_t* data, uint16_t capacity,
                                uint16_t* bytesRead);
    NTAG21x_NAKStatus getNAKStatus(
        const uint8_t* response,
        uint8_t recvLen,
        uint8_t rxLastBits
    ) const;
    NTAG21x_NAKStatus _lastNAKStatus;
};

// Testi leggibili (stringhe in flash: usabili direttamente con Serial.println).
const __FlashStringHelper* ntag21xStatusText(NTAG21x_Status status);
const __FlashStringHelper* ntag21xNakText(NTAG21x_NAKStatus nak);

static_assert((NTAG21x_Protocol::FAST_READ_MAX_PAGES * NTAG21x_Protocol::PAGE_SIZE +
               NTAG21x_Protocol::CRC_SIZE) <= NTAG21x_Protocol::MAX_FIFO_SIZE,
              "FAST_READ chunk exceeds MFRC522 FIFO capacity");

#endif
