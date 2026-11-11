#include "RC522_NTAG21X.h"
#include <string.h>

RC522_NTAG21x::RC522_NTAG21x(RC522_ISO14443A& iso)
    : _iso(&iso),
      _initialized(false),
      _lastNAKStatus(NAK_NONE) {
    memset(&_info, 0, sizeof(_info));
    _info.model = NTAG21X_UNKNOWN;
}

NTAG21x_Status RC522_NTAG21x::begin() {
    _lastNAKStatus = NAK_NONE;
    _initialized = false;
    memset(&_info, 0, sizeof(_info));
    _info.model = NTAG21X_UNKNOWN;

    if (_iso == nullptr || !_iso->isInitialized())
        return NTAG21X_NOT_INITIALIZED;

    NTAG21x_Status status = getVersion();
    _initialized = (status == NTAG21X_OK);
    return status;
}

NTAG21x_Status RC522_NTAG21x::mapISOStatus(ISO14443A_Status status) const {
    switch (status) {
        case ISO14443A_OK:              return NTAG21X_OK;
        case ISO14443A_NO_CARD:         return NTAG21X_TIMEOUT;
        case ISO14443A_TIMEOUT:         return NTAG21X_TIMEOUT;
        case ISO14443A_CRC_ERROR:       return NTAG21X_CRC_ERROR;
        case ISO14443A_COLLISION:
        case ISO14443A_PROTOCOL_ERROR:  return NTAG21X_PROTOCOL_ERROR;
        case ISO14443A_INVALID_UID:     return NTAG21X_INVALID_ARGUMENT;
        default:                        return NTAG21X_ERROR;
    }
}

NTAG21x_Status RC522_NTAG21x::appendCRC(const uint8_t* data, uint8_t length,
                                         uint8_t* output) {
    if (data == nullptr || output == nullptr || length == 0)
        return NTAG21X_INVALID_ARGUMENT;

    uint8_t crc[2] = {0, 0};
    NTAG21x_Status status = mapISOStatus(_iso->calculateCRC_A(data, length, crc));
    if (status != NTAG21X_OK) return status;

    memcpy(output, data, length);
    output[length] = crc[0];
    output[length + 1] = crc[1];
    return NTAG21X_OK;
}
//============================================================================
// Gestione NAK (Negative Acknowledgment) dei comandi NTAG21x.
//============================================================================
NTAG21x_NAKStatus RC522_NTAG21x::getNAKStatus(
    const uint8_t* response,
    uint8_t recvLen,
    uint8_t rxLastBits
) const {
    // Una risposta NAK NTAG è composta da un solo nibble.
    if (recvLen != 1 || rxLastBits != 4)
        return NAK_NONE;

    if (response == nullptr)
        return NAK_UNKNOWN;

    switch (response[0] & NTAG21x_Response::NAK_MASK) {
        case NTAG21x_Response::NAK_INVALID_ARGUMENT:
            return NAK_INVALID_ARGUMENT;

        case NTAG21x_Response::NAK_CRC_PARITY:
            return NAK_CRC_PARITY_ERROR;

        case NTAG21x_Response::NAK_AUTH_LIMIT:
            return NAK_AUTH_LIMIT_REACHED;

        case NTAG21x_Response::NAK_EEPROM_WRITE:
            return NAK_EEPROM_WRITE_ERROR;

        default:
            return NAK_UNKNOWN;
    }
}

NTAG21x_NAKStatus RC522_NTAG21x::getLastNAKStatus() const {
    return _lastNAKStatus;
}
//============================================================================
// Versione del chip NTAG21x e informazioni sul modello.
//============================================================================
NTAG21x_Status RC522_NTAG21x::getVersion() {
    const uint8_t request[1] = { NTAG21x_Command::GET_VERSION };
    uint8_t command[3];
    NTAG21x_Status status = appendCRC(request, 1, command);
    if (status != NTAG21X_OK) return status;

    uint8_t response[10] = {0};
    uint8_t recvLen = 0;
    uint8_t rxLastBits = 0;
    status = mapISOStatus(_iso->communicate(command, sizeof(command), response,
                    &recvLen, sizeof(response), 0, 0, &rxLastBits, nullptr, false));
    if (status != NTAG21X_OK) return status;
    _lastNAKStatus = getNAKStatus(response, recvLen, rxLastBits);
    if (_lastNAKStatus != NAK_NONE) return NTAG21X_NAK;
    if (recvLen != sizeof(response) || rxLastBits != 0)
        return NTAG21X_PROTOCOL_ERROR;

    uint8_t crc[2] = {0, 0};
    status = mapISOStatus(_iso->calculateCRC_A(response, 8, crc));
    if (status != NTAG21X_OK) return status;
    if (crc[0] != response[8] || crc[1] != response[9])
        return NTAG21X_CRC_ERROR;

    memcpy(_info.version, response, sizeof(_info.version));
    if (response[1] != NTAG21x_Version::VENDOR_NXP ||
        response[2] != NTAG21x_Version::PRODUCT_TYPE ||
        response[3] != NTAG21x_Version::PRODUCT_SUBTYPE ||
        response[4] != NTAG21x_Version::MAJOR_VERSION)
        return NTAG21X_UNSUPPORTED;

    switch (response[6]) {
        case NTAG21x_Version::NTAG213_SIZE_CODE:
            _info.model = NTAG213;
            _info.lastPage = NTAG21x_Memory::NTAG213_LAST_PAGE;
            break;

        case NTAG21x_Version::NTAG215_SIZE_CODE:
            _info.model = NTAG215;
            _info.lastPage = NTAG21x_Memory::NTAG215_LAST_PAGE;
            break;

        case NTAG21x_Version::NTAG216_SIZE_CODE:
            _info.model = NTAG216;
            _info.lastPage = NTAG21x_Memory::NTAG216_LAST_PAGE;
            break;
        default:
            _info.model = NTAG21X_UNKNOWN;
            return NTAG21X_UNSUPPORTED;
    }
    _info.firstUserPage = NTAG21x_Memory::PAGE_USER_START;
    _info.totalBytes =
        ((uint16_t)_info.lastPage + 1) *
        NTAG21x_Memory::PAGE_SIZE;

    _info.lastUserPage =
    _info.lastPage - NTAG21x_Memory::RESERVED_PAGES;

    _info.userBytes =((uint16_t)_info.lastUserPage - _info.firstUserPage + 1) *  NTAG21x_Memory::PAGE_SIZE;

    _info.dynamicLockPage = _info.lastPage - 4;
    _info.configStartPage = _info.lastPage - 3;
    _info.pwdPage = _info.lastPage - 1;
    _info.packPage = _info.lastPage;
    return NTAG21X_OK;
}

NTAG21x_Status RC522_NTAG21x::readPage(
    uint8_t page, uint8_t data[NTAG21x_Protocol::PAGE_SIZE]) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (data == nullptr || page > _info.lastPage)
        return NTAG21X_INVALID_ARGUMENT;

    const uint8_t request[2] = { NTAG21x_Command::READ, page };
    uint8_t command[4];
    NTAG21x_Status status = appendCRC(request, 2, command);
    if (status != NTAG21X_OK) return status;

    // READ: 16 byte dati (4 pagine) + CRC_A.
    uint8_t response[18] = {0};
    uint8_t recvLen = 0;
    uint8_t rxLastBits = 0;
    status = mapISOStatus(_iso->communicate(command, sizeof(command), response,
                    &recvLen, sizeof(response), 0, 0, &rxLastBits, nullptr, false));
    if (status != NTAG21X_OK) return status;
    _lastNAKStatus = getNAKStatus(response, recvLen, rxLastBits);
    if (_lastNAKStatus != NAK_NONE) return NTAG21X_NAK;
    if (recvLen != sizeof(response) || rxLastBits != 0)
        return NTAG21X_PROTOCOL_ERROR;

    uint8_t crc[2] = {0, 0};
    status = mapISOStatus(_iso->calculateCRC_A(response, 16, crc));
    if (status != NTAG21X_OK) return status;
    if (crc[0] != response[16] || crc[1] != response[17])
        return NTAG21X_CRC_ERROR;

    memcpy(data, response, NTAG21x_Protocol::PAGE_SIZE);
    return NTAG21X_OK;
}

NTAG21x_Status RC522_NTAG21x::fastRead(uint8_t startPage, uint8_t endPage,
                                       uint8_t* data, uint16_t capacity,
                                       uint16_t* bytesRead) {
    if (bytesRead != nullptr) *bytesRead = 0;
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (data == nullptr || startPage > endPage || endPage > _info.lastPage)
        return NTAG21X_INVALID_ARGUMENT;

    const uint16_t pageCount = (uint16_t)endPage - startPage + 1;
    if (pageCount > NTAG21x_Protocol::FAST_READ_MAX_PAGES)
        return NTAG21X_INVALID_ARGUMENT;

    const uint16_t dataLength = pageCount * NTAG21x_Protocol::PAGE_SIZE;
    const uint16_t responseLength = dataLength + NTAG21x_Protocol::CRC_SIZE;
    if (capacity < dataLength) return NTAG21X_BUFFER_TOO_SMALL;

    const uint8_t request[3] = { NTAG21x_Command::FAST_READ, startPage, endPage };
    uint8_t command[5];
    NTAG21x_Status status = appendCRC(request, 3, command);
    if (status != NTAG21X_OK) return status;

    uint8_t response[NTAG21x_Protocol::MAX_FIFO_SIZE] = {0};
    uint8_t recvLen = 0;
    uint8_t rxLastBits = 0;
    status = mapISOStatus(_iso->communicate(command, sizeof(command), response,
                    &recvLen, sizeof(response), 0, 0, &rxLastBits, nullptr, false));
    if (status != NTAG21X_OK) return status;
        _lastNAKStatus = getNAKStatus(response, recvLen, rxLastBits);
    if (_lastNAKStatus != NAK_NONE) return NTAG21X_NAK;
    if (recvLen != responseLength || rxLastBits != 0)
        return NTAG21X_PROTOCOL_ERROR;

    uint8_t crc[2] = {0, 0};
    status = mapISOStatus(_iso->calculateCRC_A(response, dataLength, crc));
    if (status != NTAG21X_OK) return status;
    if (crc[0] != response[dataLength] || crc[1] != response[dataLength + 1])
        return NTAG21X_CRC_ERROR;

    memcpy(data, response, dataLength);
    if (bytesRead != nullptr) *bytesRead = dataLength;
    return NTAG21X_OK;
}

NTAG21x_Status RC522_NTAG21x::readRangeRaw(uint8_t startPage, uint8_t endPage,
                                           uint8_t* data, uint16_t capacity,
                                           uint16_t* bytesRead) {
    if (bytesRead != nullptr) *bytesRead = 0;
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (data == nullptr || startPage > endPage || endPage > _info.lastPage)
        return NTAG21X_INVALID_ARGUMENT;

    const uint16_t needed = ((uint16_t)endPage - startPage + 1) *
                            NTAG21x_Protocol::PAGE_SIZE;
    if (capacity < needed) return NTAG21X_BUFFER_TOO_SMALL;

    uint16_t total = 0;
    uint16_t current = startPage;
    while (current <= endPage) {
        uint16_t chunkEnd = current + NTAG21x_Protocol::FAST_READ_MAX_PAGES - 1;
        if (chunkEnd > endPage) chunkEnd = endPage;
        uint16_t chunkRead = 0;
        NTAG21x_Status status = fastRead((uint8_t)current, (uint8_t)chunkEnd,
                                         data + total, capacity - total, &chunkRead);
        if (status != NTAG21X_OK) return status;
        total += chunkRead;
        current = chunkEnd + 1;
    }
    if (bytesRead != nullptr) *bytesRead = total;
    return NTAG21X_OK;
}

NTAG21x_Status RC522_NTAG21x::readDump(NTAG21x_Dump& dump) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    memset(&dump, 0, sizeof(dump));
    dump.lastPage = _info.lastPage;

    uint16_t page = 0;
    while (page <= _info.lastPage) {
        uint16_t endPage = page + NTAG21x_Protocol::FAST_READ_MAX_PAGES - 1;
        if (endPage > _info.lastPage) endPage = _info.lastPage;
        const uint8_t count = (uint8_t)(endPage - page + 1);
        const uint16_t chunkSize = (uint16_t)count * NTAG21x_Protocol::PAGE_SIZE;
        uint16_t received = 0;

        NTAG21x_Status status = fastRead((uint8_t)page, (uint8_t)endPage,
                    &dump.memory[page][0], chunkSize, &received);
        if (status == NTAG21X_OK) {
            for (uint8_t i = 0; i < count; ++i)
                dump.pageValid[page + i] = true;
        } else if (status == NTAG21X_NAK) {
            // Se un intervallo è protetto, tenta le singole pagine per
            // conservare nel dump tutte quelle che restano leggibili.
            for (uint16_t p = page; p <= endPage; ++p) {
                NTAG21x_Status one = readPage((uint8_t)p, dump.memory[p]);
                if (one == NTAG21X_OK) dump.pageValid[p] = true;
            }
        } else {
            return status;
        }
        page = endPage + 1;
    }

    dump.complete = true;
    for (uint16_t p = 0; p <= _info.lastPage; ++p) {
        if (!dump.pageValid[p]) {
            dump.complete = false;
            break;
        }
    }
    return dump.complete ? NTAG21X_OK : NTAG21X_PARTIAL_DUMP;
}

const NTAG21x_Info& RC522_NTAG21x::getInfo() const {
    return _info;
}

bool RC522_NTAG21x::isDetected() const {
    return _initialized && _info.model != NTAG21X_UNKNOWN;
}

//============================================================================
// Scrittura, autenticazione e configurazione globale.
//============================================================================

// Interpreta la risposta a 4 bit a un comando di scrittura: ACK = 0xA.
NTAG21x_Status RC522_NTAG21x::checkAck(const uint8_t* response, uint8_t recvLen,
                                       uint8_t rxLastBits) {
    if (recvLen == 1 && rxLastBits == 4 &&
        (response[0] & NTAG21x_Response::NAK_MASK) == NTAG21x_Response::ACK) {
        _lastNAKStatus = NAK_NONE;
        return NTAG21X_OK;
    }
    _lastNAKStatus = getNAKStatus(response, recvLen, rxLastBits);
    if (_lastNAKStatus != NAK_NONE) return NTAG21X_NAK;
    return NTAG21X_PROTOCOL_ERROR;
}

// WRITE: 1 pagina (4 byte). Pagine 0-1 (UID) non scrivibili.
// Attenzione: sulle pagine 2 (lock bit, byte 2-3) e 3 (CC) il chip mette in OR i
// dati con il contenuto: i bit a 1 non tornano più a 0 (irreversibile).
NTAG21x_Status RC522_NTAG21x::writePage(
    uint8_t page, const uint8_t data[NTAG21x_Protocol::PAGE_SIZE]) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (data == nullptr || page < 2 || page > _info.lastPage)
        return NTAG21X_INVALID_ARGUMENT;

    uint8_t request[6] = { NTAG21x_Command::WRITE, page,
                           data[0], data[1], data[2], data[3] };
    uint8_t command[8];
    NTAG21x_Status status = appendCRC(request, sizeof(request), command);
    if (status != NTAG21X_OK) return status;

    uint8_t response[2] = {0};
    uint8_t recvLen = 0;
    uint8_t rxLastBits = 0;
    status = mapISOStatus(_iso->communicate(command, sizeof(command), response,
                    &recvLen, sizeof(response), 0, 0, &rxLastBits, nullptr, false));
    if (status != NTAG21X_OK) return status;
    return checkAck(response, recvLen, rxLastBits);
}

// PWD_AUTH: dopo un NAK il tag può uscire dallo stato ACTIVE; in quel caso
// ripetere wakeup() + selectCard() (e begin() se si cambia tag).
NTAG21x_Status RC522_NTAG21x::authenticate(const uint8_t password[4], uint8_t pack[2]) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (password == nullptr) return NTAG21X_INVALID_ARGUMENT;

    const uint8_t request[5] = { NTAG21x_Command::PWD_AUTH,
                                 password[0], password[1], password[2], password[3] };
    uint8_t command[7];
    NTAG21x_Status status = appendCRC(request, sizeof(request), command);
    if (status != NTAG21X_OK) return status;

    uint8_t response[4] = {0};   // PACK (2) + CRC_A (2)
    uint8_t recvLen = 0;
    uint8_t rxLastBits = 0;
    status = mapISOStatus(_iso->communicate(command, sizeof(command), response,
                    &recvLen, sizeof(response), 0, 0, &rxLastBits, nullptr, false));
    if (status != NTAG21X_OK) return status;

    _lastNAKStatus = getNAKStatus(response, recvLen, rxLastBits);
    if (_lastNAKStatus == NAK_AUTH_LIMIT_REACHED) return NTAG21X_AUTH_LOCKED;
    if (_lastNAKStatus != NAK_NONE) return NTAG21X_AUTH_FAILED;
    if (recvLen != sizeof(response) || rxLastBits != 0)
        return NTAG21X_PROTOCOL_ERROR;

    uint8_t crc[2] = {0, 0};
    status = mapISOStatus(_iso->calculateCRC_A(response, 2, crc));
    if (status != NTAG21X_OK) return status;
    if (crc[0] != response[2] || crc[1] != response[3]) return NTAG21X_CRC_ERROR;

    if (pack != nullptr) { pack[0] = response[0]; pack[1] = response[1]; }
    return NTAG21X_OK;
}

//----------------------------------------------------------------------------
// Dump della configurazione globale
//----------------------------------------------------------------------------
NTAG21x_Status RC522_NTAG21x::readConfig(NTAG21x_Config& cfg) {
    memset(&cfg, 0, sizeof(cfg));
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;

    // Pagine 2-3: static lock bytes + capability container.
    uint8_t buf[12] = {0};
    uint16_t got = 0;
    NTAG21x_Status status = fastRead(2, 3, buf, sizeof(buf), &got);
    if (status != NTAG21X_OK) return status;
    cfg.staticLock[0] = buf[2];
    cfg.staticLock[1] = buf[3];
    memcpy(cfg.capabilityContainer, &buf[4], 4);
    cfg.lockValid = true;   // lock statici e CC disponibili

    // Dynamic lock + CFG0 + CFG1 (PWD/PACK non vengono richiesti: leggono 0).
    status = fastRead(_info.dynamicLockPage, _info.configStartPage + 1,
                      buf, sizeof(buf), &got);
    if (status != NTAG21X_OK) return status;   // NAK se PROT=1 e non autenticati
    memcpy(cfg.dynamicLock, &buf[0], 3);
    memcpy(cfg.cfg0, &buf[4], 4);
    memcpy(cfg.cfg1, &buf[8], 4);
    cfg.configValid = true;

    cfg.mirrorMode = (NTAG21x_MirrorMode)((cfg.cfg0[0] >> NTAG21x_Cfg::MIRROR_CONF_SHIFT) & 0x03);
    cfg.mirrorByte = (cfg.cfg0[0] >> NTAG21x_Cfg::MIRROR_BYTE_SHIFT) & 0x03;
    cfg.strongModulation = (cfg.cfg0[0] & NTAG21x_Cfg::STRG_MOD_EN) != 0;
    cfg.mirrorPage = cfg.cfg0[2];
    cfg.auth0 = cfg.cfg0[3];

    cfg.readProtected = (cfg.cfg1[0] & NTAG21x_Cfg::ACCESS_PROT) != 0;
    cfg.configLocked = (cfg.cfg1[0] & NTAG21x_Cfg::ACCESS_CFGLCK) != 0;
    cfg.nfcCounterEnabled = (cfg.cfg1[0] & NTAG21x_Cfg::ACCESS_NFC_CNT_EN) != 0;
    cfg.nfcCounterPwdProtected = (cfg.cfg1[0] & NTAG21x_Cfg::ACCESS_NFC_CNT_PWD_PROT) != 0;
    cfg.authLimit = cfg.cfg1[0] & NTAG21x_Cfg::ACCESS_AUTHLIM_MASK;
    cfg.vctid = cfg.cfg1[1];
    return NTAG21X_OK;
}

static void printHexBytes(Print& out, const uint8_t* d, uint8_t n) {
    for (uint8_t i = 0; i < n; ++i) {
        if (d[i] < 0x10) out.print('0');
        out.print(d[i], HEX);
        if (i + 1 < n) out.print(' ');
    }
}

void RC522_NTAG21x::printConfig(Print& out, const NTAG21x_Config& cfg) const {
    const char* name = "sconosciuto";
    switch (_info.model) {
        case NTAG213: name = "NTAG213"; break;
        case NTAG215: name = "NTAG215"; break;
        case NTAG216: name = "NTAG216"; break;
        default: break;
    }
    out.print(F("Chip: ")); out.println(name);
    out.print(F("Pagine totali: ")); out.print(_info.lastPage + 1);
    out.print(F("  user: ")); out.print(_info.firstUserPage);
    out.print('-'); out.print(_info.lastUserPage);
    out.print(F(" (")); out.print(_info.userBytes); out.println(F(" byte)"));

    if (!cfg.lockValid) { out.println(F("Configurazione non letta")); return; }

    out.print(F("Static lock  (pag 2):  ")); printHexBytes(out, cfg.staticLock, 2); out.println();
    out.print(F("Cap. container (pag 3): ")); printHexBytes(out, cfg.capabilityContainer, 4); out.println();
    out.print(F("Dynamic lock (pag ")); out.print(_info.dynamicLockPage);
    out.print(F("): ")); printHexBytes(out, cfg.dynamicLock, 3); out.println();

    if (!cfg.configValid) { out.println(F("CFG0/CFG1 non leggibili (autenticarsi)")); return; }

    out.print(F("CFG0 (pag ")); out.print(_info.configStartPage); out.print(F("): "));
    printHexBytes(out, cfg.cfg0, 4); out.println();
    out.print(F("CFG1 (pag ")); out.print(_info.configStartPage + 1); out.print(F("): "));
    printHexBytes(out, cfg.cfg1, 4); out.println();

    static const char* const mirrorNames[4] = { "OFF", "UID", "NFC counter", "UID + NFC counter" };
    out.print(F("ASCII mirror:       ")); out.println(mirrorNames[cfg.mirrorMode & 3]);
    if (cfg.mirrorMode != NTAG21X_MIRROR_OFF) {
        out.print(F("  mirror start:     pag ")); out.print(cfg.mirrorPage);
        out.print(F(", byte ")); out.println(cfg.mirrorByte);
    }
    out.print(F("Strong modulation:  ")); out.println(cfg.strongModulation ? F("ON") : F("OFF"));
    out.print(F("AUTH0:              "));
    if (cfg.auth0 == NTAG21x_Cfg::AUTH0_DISABLED || cfg.auth0 > _info.lastPage) out.println(F("nessuna protezione"));
    else { out.print(F("pag ")); out.print(cfg.auth0); out.println(F(" in poi")); }
    out.print(F("Protezione:         ")); out.println(cfg.readProtected ? F("lettura + scrittura") : F("solo scrittura"));
    out.print(F("Config bloccata:    ")); out.println(cfg.configLocked ? F("SI (CFGLCK)") : F("no"));
    out.print(F("NFC counter:        ")); out.println(cfg.nfcCounterEnabled ? F("ON") : F("OFF"));
    out.print(F("Counter con PWD:    ")); out.println(cfg.nfcCounterPwdProtected ? F("SI") : F("no"));
    out.print(F("AUTHLIM:            ")); out.println(cfg.authLimit);
    out.print(F("CFG1 byte 1:        0x")); out.println(cfg.vctid, HEX);
}

//----------------------------------------------------------------------------
// Modifica della configurazione (read-modify-write + verifica)
//----------------------------------------------------------------------------
NTAG21x_Status RC522_NTAG21x::checkConfigUnlocked() {
    uint8_t cfg1[NTAG21x_Protocol::PAGE_SIZE];
    NTAG21x_Status status = readPage(_info.configStartPage + NTAG21x_Cfg::PAGE_CFG1, cfg1);
    if (status != NTAG21X_OK) return status;   // NAK: serve authenticate()
    if (cfg1[0] & NTAG21x_Cfg::ACCESS_CFGLCK) return NTAG21X_LOCKED;
    return NTAG21X_OK;
}

NTAG21x_Status RC522_NTAG21x::modifyConfigPage(uint8_t pageOffset, const uint8_t mask[4],
                                               const uint8_t value[4]) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (pageOffset > NTAG21x_Cfg::PAGE_CFG1) return NTAG21X_INVALID_ARGUMENT;

    NTAG21x_Status status = checkConfigUnlocked();
    if (status != NTAG21X_OK) return status;

    const uint8_t page = _info.configStartPage + pageOffset;
    uint8_t current[4];
    status = readPage(page, current);
    if (status != NTAG21X_OK) return status;

    uint8_t next[4];
    for (uint8_t i = 0; i < 4; ++i)
        next[i] = (uint8_t)((current[i] & ~mask[i]) | (value[i] & mask[i]));

    // Niente scrittura se non cambia nulla: risparmia cicli EEPROM.
    if (memcmp(current, next, 4) == 0) return NTAG21X_OK;

    status = writePage(page, next);
    if (status != NTAG21X_OK) return status;

    uint8_t check[4];
    status = readPage(page, check);
    // Se la modifica ha appena attivato PROT, la rilettura può dare NAK:
    // l'ACK della scrittura è già stato ricevuto.
    if (status == NTAG21X_NAK) return NTAG21X_OK;
    if (status != NTAG21X_OK) return status;
    return memcmp(check, next, 4) == 0 ? NTAG21X_OK : NTAG21X_VERIFY_FAILED;
}

NTAG21x_Status RC522_NTAG21x::modifyConfigByte(uint8_t pageOffset, uint8_t byteIndex,
                                               uint8_t mask, uint8_t value) {
    uint8_t m[4] = {0, 0, 0, 0};
    uint8_t v[4] = {0, 0, 0, 0};
    m[byteIndex] = mask;
    v[byteIndex] = value;
    return modifyConfigPage(pageOffset, m, v);
}

NTAG21x_Status RC522_NTAG21x::setStrongModulation(bool enable) {
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG0, 0, NTAG21x_Cfg::STRG_MOD_EN,
                            enable ? NTAG21x_Cfg::STRG_MOD_EN : 0);
}

NTAG21x_Status RC522_NTAG21x::setMirror(NTAG21x_MirrorMode mode, uint8_t page,
                                        uint8_t byteOffset) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (mode > NTAG21X_MIRROR_UID_COUNTER) return NTAG21X_INVALID_ARGUMENT;

    uint8_t mirrorByte = 0;
    uint8_t mirrorPage = 0;   // 0 = mirror disattivato

    if (mode != NTAG21X_MIRROR_OFF) {
        if (byteOffset > 3 || page < NTAG21x_Memory::PAGE_USER_START ||
            page > _info.lastUserPage)
            return NTAG21X_INVALID_ARGUMENT;

        uint8_t length = NTAG21x_Cfg::MIRROR_LEN_UID;
        if (mode == NTAG21X_MIRROR_COUNTER) length = NTAG21x_Cfg::MIRROR_LEN_COUNTER;
        if (mode == NTAG21X_MIRROR_UID_COUNTER) length = NTAG21x_Cfg::MIRROR_LEN_BOTH;

        // Datasheet tab. 13/16/19: il mirror deve finire prima dell'ultimo byte
        // della memoria utente (es. UID: pagina max = ultima utente - 3, byte max 1).
        const uint16_t end = (uint16_t)page * NTAG21x_Memory::PAGE_SIZE + byteOffset + length;
        const uint16_t limit = ((uint16_t)_info.lastUserPage + 1) * NTAG21x_Memory::PAGE_SIZE;
        if (end + 1 > limit) return NTAG21X_INVALID_ARGUMENT;

        mirrorByte = byteOffset;
        mirrorPage = page;
    }

    uint8_t mask[4]  = { NTAG21x_Cfg::MIRROR_FIELDS_MASK, 0, 0xFF, 0 };
    uint8_t value[4] = { (uint8_t)((mode << NTAG21x_Cfg::MIRROR_CONF_SHIFT) |
                                   (mirrorByte << NTAG21x_Cfg::MIRROR_BYTE_SHIFT)),
                         0, mirrorPage, 0 };
    return modifyConfigPage(NTAG21x_Cfg::PAGE_CFG0, mask, value);
}

NTAG21x_Status RC522_NTAG21x::setAuth0(uint8_t firstProtectedPage) {
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG0, 3, 0xFF, firstProtectedPage);
}

NTAG21x_Status RC522_NTAG21x::setReadProtection(bool enable) {
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG1, 0, NTAG21x_Cfg::ACCESS_PROT,
                            enable ? NTAG21x_Cfg::ACCESS_PROT : 0);
}

NTAG21x_Status RC522_NTAG21x::setNfcCounter(bool enable) {
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG1, 0, NTAG21x_Cfg::ACCESS_NFC_CNT_EN,
                            enable ? NTAG21x_Cfg::ACCESS_NFC_CNT_EN : 0);
}

NTAG21x_Status RC522_NTAG21x::setNfcCounterPasswordProtection(bool enable) {
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG1, 0, NTAG21x_Cfg::ACCESS_NFC_CNT_PWD_PROT,
                            enable ? NTAG21x_Cfg::ACCESS_NFC_CNT_PWD_PROT : 0);
}

NTAG21x_Status RC522_NTAG21x::setAuthLimit(uint8_t limit) {
    if (limit > NTAG21x_Cfg::ACCESS_AUTHLIM_MASK) return NTAG21X_INVALID_ARGUMENT;
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG1, 0, NTAG21x_Cfg::ACCESS_AUTHLIM_MASK, limit);
}

NTAG21x_Status RC522_NTAG21x::setPassword(const uint8_t password[4]) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (password == nullptr) return NTAG21X_INVALID_ARGUMENT;
    // PWD e PACK restano scrivibili anche con CFGLCK attivo (datasheet 8.8.1):
    // per questo conviene impostare AUTH0 = pwdPage dopo averli scritti.
    // PWD non è leggibile: nessuna verifica possibile oltre all'ACK.
    return writePage(_info.pwdPage, password);
}

NTAG21x_Status RC522_NTAG21x::setPack(const uint8_t pack[2]) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (pack == nullptr) return NTAG21X_INVALID_ARGUMENT;
    const uint8_t data[4] = { pack[0], pack[1], 0x00, 0x00 };  // byte 2-3 RFUI
    return writePage(_info.packPage, data);
}

// CFGLCK: blocca per sempre la scrittura di CFG0 e CFG1 (PWD e PACK restano
// scrivibili). Il datasheet precisa che il blocco diventa effettivo dopo un
// power cycle del tag; il bit è comunque rileggibile subito.
NTAG21x_Status RC522_NTAG21x::lockConfig(bool confirm) {
    if (!confirm) return NTAG21X_INVALID_ARGUMENT;
    return modifyConfigByte(NTAG21x_Cfg::PAGE_CFG1, 0, NTAG21x_Cfg::ACCESS_CFGLCK,
                            NTAG21x_Cfg::ACCESS_CFGLCK);
}

//============================================================================
// Testi leggibili per stati e NAK
//============================================================================
const __FlashStringHelper* ntag21xStatusText(NTAG21x_Status status) {
    switch (status) {
        case NTAG21X_OK:               return F("OK");
        case NTAG21X_NOT_INITIALIZED:  return F("non inizializzato");
        case NTAG21X_UNSUPPORTED:      return F("tag non supportato");
        case NTAG21X_TIMEOUT:          return F("timeout (nessuna risposta)");
        case NTAG21X_NAK:              return F("NAK dal tag");
        case NTAG21X_CRC_ERROR:        return F("errore CRC");
        case NTAG21X_PROTOCOL_ERROR:   return F("errore di protocollo");
        case NTAG21X_INVALID_ARGUMENT: return F("argomento non valido");
        case NTAG21X_BUFFER_TOO_SMALL: return F("buffer troppo piccolo");
        case NTAG21X_PARTIAL_DUMP:     return F("dump parziale");
        case NTAG21X_AUTH_FAILED:      return F("autenticazione fallita");
        case NTAG21X_AUTH_LOCKED:      return F("limite tentativi password raggiunto");
        case NTAG21X_VERIFY_FAILED:    return F("verifica post-scrittura fallita");
        case NTAG21X_LOCKED:           return F("configurazione bloccata (CFGLCK)");
        case NTAG21X_MIRROR_CONFLICT:  return F("conflitto con l'ASCII mirror");
        case NTAG21X_ERROR:
        default:                       return F("errore generico");
    }
}

// Il datasheet (tab. 23) non elenca un codice specifico per "accesso negato":
// si osserva in pratica 0x0, che coincide con "argomento non valido".
const __FlashStringHelper* ntag21xNakText(NTAG21x_NAKStatus nak) {
    switch (nak) {
        case NAK_NONE:                 return F("nessun NAK");
        case NAK_INVALID_ARGUMENT:     return F("argomento non valido o accesso negato");
        case NAK_CRC_PARITY_ERROR:     return F("errore CRC/parita'");
        case NAK_AUTH_LIMIT_REACHED:   return F("limite tentativi password raggiunto");
        case NAK_EEPROM_WRITE_ERROR:   return F("errore di scrittura EEPROM");
        case NAK_UNKNOWN:
        default:                       return F("NAK sconosciuto");
    }
}

//============================================================================
// Comando generico "richiesta + dati di risposta con CRC"
//============================================================================
NTAG21x_Status RC522_NTAG21x::commandWithData(const uint8_t* request, uint8_t requestLen,
                                              uint8_t* data, uint8_t dataLen) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (request == nullptr || data == nullptr || requestLen == 0 || requestLen > 6 ||
        dataLen == 0 || dataLen + NTAG21x_Protocol::CRC_SIZE > NTAG21x_Protocol::MAX_FIFO_SIZE)
        return NTAG21X_INVALID_ARGUMENT;

    uint8_t command[8];
    NTAG21x_Status status = appendCRC(request, requestLen, command);
    if (status != NTAG21X_OK) return status;

    uint8_t response[NTAG21x_Protocol::MAX_FIFO_SIZE] = {0};
    uint8_t recvLen = 0;
    uint8_t rxLastBits = 0;
    status = mapISOStatus(_iso->communicate(command, requestLen + NTAG21x_Protocol::CRC_SIZE,
                    response, &recvLen, sizeof(response), 0, 0, &rxLastBits, nullptr, false));
    if (status != NTAG21X_OK) return status;

    _lastNAKStatus = getNAKStatus(response, recvLen, rxLastBits);
    if (_lastNAKStatus != NAK_NONE) return NTAG21X_NAK;
    if (recvLen != dataLen + NTAG21x_Protocol::CRC_SIZE || rxLastBits != 0)
        return NTAG21X_PROTOCOL_ERROR;

    uint8_t crc[2] = {0, 0};
    status = mapISOStatus(_iso->calculateCRC_A(response, dataLen, crc));
    if (status != NTAG21X_OK) return status;
    if (crc[0] != response[dataLen] || crc[1] != response[dataLen + 1])
        return NTAG21X_CRC_ERROR;

    memcpy(data, response, dataLen);
    return NTAG21X_OK;
}

//============================================================================
// Contatore NFC e firma di originalità
//============================================================================
// READ_CNT: contatore a 24 bit, LSByte per primo. Con NFC_CNT_PWD_PROT=1 serve
// prima authenticate(), altrimenti il tag risponde NAK.
NTAG21x_Status RC522_NTAG21x::readCounter(uint32_t& value) {
    value = 0;
    const uint8_t request[2] = { NTAG21x_Command::READ_CNT, NTAG21x_Protocol::COUNTER_ADDR };
    uint8_t data[NTAG21x_Protocol::COUNTER_SIZE] = {0};
    NTAG21x_Status status = commandWithData(request, sizeof(request), data, sizeof(data));
    if (status != NTAG21X_OK) return status;
    value = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
    return NTAG21X_OK;
}

// READ_SIG: firma ECDSA (secp128r1) a 32 byte. La verifica richiede la chiave
// pubblica NXP (AN11350) e non è implementata qui.
NTAG21x_Status RC522_NTAG21x::readSignature(uint8_t signature[NTAG21x_Protocol::SIGNATURE_SIZE]) {
    if (signature == nullptr) return NTAG21X_INVALID_ARGUMENT;
    const uint8_t request[2] = { NTAG21x_Command::READ_SIG, NTAG21x_Protocol::SIGNATURE_ADDR };
    return commandWithData(request, sizeof(request), signature, NTAG21x_Protocol::SIGNATURE_SIZE);
}

//============================================================================
// Accesso a byte nella memoria utente (offset 0 = pagina 4, byte 0)
//============================================================================
NTAG21x_Status RC522_NTAG21x::readBytes(uint16_t offset, uint8_t* data, uint16_t length) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (length == 0) return NTAG21X_OK;
    if (data == nullptr || offset >= _info.userBytes || length > _info.userBytes - offset)
        return NTAG21X_INVALID_ARGUMENT;

    uint8_t buffer[NTAG21x_Protocol::FAST_READ_MAX_PAGES * NTAG21x_Protocol::PAGE_SIZE];
    uint16_t done = 0;
    while (done < length) {
        const uint16_t absolute = offset + done;
        const uint8_t page = _info.firstUserPage + absolute / NTAG21x_Protocol::PAGE_SIZE;
        const uint8_t skip = absolute % NTAG21x_Protocol::PAGE_SIZE;
        uint16_t pages = (length - done + skip + NTAG21x_Protocol::PAGE_SIZE - 1) /
                         NTAG21x_Protocol::PAGE_SIZE;
        if (pages > NTAG21x_Protocol::FAST_READ_MAX_PAGES) pages = NTAG21x_Protocol::FAST_READ_MAX_PAGES;

        uint16_t got = 0;
        NTAG21x_Status status = fastRead(page, (uint8_t)(page + pages - 1),
                                         buffer, sizeof(buffer), &got);
        if (status != NTAG21X_OK) return status;

        uint16_t take = got - skip;
        if (take > length - done) take = length - done;
        memcpy(data + done, buffer + skip, take);
        done += take;
    }
    return NTAG21X_OK;
}

// Pagine coperte dall'ASCII mirror (se attivo). Se CFG0 non è leggibile (NAK)
// il mirror è considerato non attivo.
NTAG21x_Status RC522_NTAG21x::getMirrorSpan(bool& active, uint8_t& firstPage, uint8_t& lastPage) {
    active = false;
    firstPage = lastPage = 0;

    uint8_t cfg0[NTAG21x_Protocol::PAGE_SIZE];
    NTAG21x_Status status = readPage(_info.configStartPage, cfg0);
    if (status == NTAG21X_NAK) return NTAG21X_OK;
    if (status != NTAG21X_OK) return status;

    const uint8_t mode = (cfg0[0] >> NTAG21x_Cfg::MIRROR_CONF_SHIFT) & 0x03;
    const uint8_t byteOffset = (cfg0[0] >> NTAG21x_Cfg::MIRROR_BYTE_SHIFT) & 0x03;
    const uint8_t page = cfg0[2];
    if (mode == NTAG21X_MIRROR_OFF || page < NTAG21x_Memory::PAGE_USER_START) return NTAG21X_OK;

    uint8_t length = NTAG21x_Cfg::MIRROR_LEN_UID;
    if (mode == NTAG21X_MIRROR_COUNTER) length = NTAG21x_Cfg::MIRROR_LEN_COUNTER;
    if (mode == NTAG21X_MIRROR_UID_COUNTER) length = NTAG21x_Cfg::MIRROR_LEN_BOTH;

    const uint16_t endByte = (uint16_t)page * NTAG21x_Memory::PAGE_SIZE + byteOffset + length - 1;
    if (endByte >= ((uint16_t)_info.lastUserPage + 1) * NTAG21x_Memory::PAGE_SIZE) return NTAG21X_OK;

    active = true;
    firstPage = page;
    lastPage = (uint8_t)(endByte / NTAG21x_Memory::PAGE_SIZE);
    return NTAG21X_OK;
}

// Scrittura a byte con read-modify-write sulle pagine parziali.
//  - salta le pagine già uguali (risparmia cicli EEPROM);
//  - rilegge e verifica ogni pagina scritta (verify=true);
//  - le pagine nell'area dell'ASCII mirror si leggono "virtuali": una scrittura
//    parziale lì copierebbe l'ASCII nella memoria fisica, quindi è rifiutata
//    (NTAG21X_MIRROR_CONFLICT). Scrivere pagine intere è invece ammesso.
NTAG21x_Status RC522_NTAG21x::writeBytes(uint16_t offset, const uint8_t* data,
                                         uint16_t length, bool verify) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (length == 0) return NTAG21X_OK;
    if (data == nullptr || offset >= _info.userBytes || length > _info.userBytes - offset)
        return NTAG21X_INVALID_ARGUMENT;

    bool mirrorActive = false;
    uint8_t mirrorFirst = 0, mirrorLast = 0;
    NTAG21x_Status status = getMirrorSpan(mirrorActive, mirrorFirst, mirrorLast);
    if (status != NTAG21X_OK) return status;

    uint16_t done = 0;
    while (done < length) {
        const uint16_t absolute = offset + done;
        const uint8_t page = _info.firstUserPage + absolute / NTAG21x_Protocol::PAGE_SIZE;
        const uint8_t skip = absolute % NTAG21x_Protocol::PAGE_SIZE;
        uint16_t chunk = NTAG21x_Protocol::PAGE_SIZE - skip;
        if (chunk > length - done) chunk = length - done;
        const bool inMirror = mirrorActive && page >= mirrorFirst && page <= mirrorLast;

        uint8_t current[NTAG21x_Protocol::PAGE_SIZE] = {0};
        if (!inMirror) {
            status = readPage(page, current);
            if (status != NTAG21X_OK) return status;
        }

        uint8_t next[NTAG21x_Protocol::PAGE_SIZE];
        if (chunk == NTAG21x_Protocol::PAGE_SIZE) {
            memcpy(next, data + done, NTAG21x_Protocol::PAGE_SIZE);
        } else {
            if (inMirror) return NTAG21X_MIRROR_CONFLICT;
            memcpy(next, current, NTAG21x_Protocol::PAGE_SIZE);
            memcpy(next + skip, data + done, chunk);
        }

        if (inMirror || memcmp(current, next, NTAG21x_Protocol::PAGE_SIZE) != 0) {
            status = writePage(page, next);
            if (status != NTAG21X_OK) return status;
            if (verify && !inMirror) {
                uint8_t check[NTAG21x_Protocol::PAGE_SIZE];
                status = readPage(page, check);
                if (status != NTAG21X_OK) return status;
                if (memcmp(check, next, NTAG21x_Protocol::PAGE_SIZE) != 0)
                    return NTAG21X_VERIFY_FAILED;
            }
        }
        done += chunk;
    }
    return NTAG21X_OK;
}

//============================================================================
// Password: procedura completa
//============================================================================
// Ordine (datasheet 8.8): PWD e PACK -> verifica con PWD_AUTH -> AUTHLIM e PROT
// -> AUTH0 per ultimo, così un errore a metà non lascia il tag bloccato.
// firstProtectedPage = pwdPage protegge solo PWD/PACK (consigliato dal datasheet
// dopo aver scritto la password); un valore più basso protegge anche la memoria.
NTAG21x_Status RC522_NTAG21x::enablePasswordProtection(const uint8_t password[4],
                                                       const uint8_t pack[2],
                                                       uint8_t firstProtectedPage,
                                                       bool protectRead,
                                                       uint8_t authLimit) {
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;
    if (password == nullptr || pack == nullptr || firstProtectedPage > _info.lastPage ||
        authLimit > NTAG21x_Cfg::ACCESS_AUTHLIM_MASK)
        return NTAG21X_INVALID_ARGUMENT;

    // Con CFGLCK attivo AUTH0 non è più modificabile: meglio non toccare nulla.
    NTAG21x_Status status = checkConfigUnlocked();
    if (status != NTAG21X_OK) return status;

    status = setPassword(password);
    if (status != NTAG21X_OK) return status;
    status = setPack(pack);
    if (status != NTAG21X_OK) return status;

    // La password appena scritta deve funzionare prima di attivare la protezione.
    uint8_t receivedPack[2] = {0, 0};
    status = authenticate(password, receivedPack);
    if (status != NTAG21X_OK) return status;
    if (receivedPack[0] != pack[0] || receivedPack[1] != pack[1]) return NTAG21X_VERIFY_FAILED;

    status = setAuthLimit(authLimit);
    if (status != NTAG21X_OK) return status;
    status = setReadProtection(protectRead);
    if (status != NTAG21X_OK) return status;
    return setAuth0(firstProtectedPage);
}

// Richiede di essere autenticati se la protezione è attiva. La password resta
// scritta nel tag (non è leggibile né cancellabile): serve solo a non usarla.
NTAG21x_Status RC522_NTAG21x::disablePasswordProtection() {
    NTAG21x_Status status = setAuth0(NTAG21x_Cfg::AUTH0_DISABLED);
    if (status != NTAG21X_OK) return status;
    status = setReadProtection(false);
    if (status != NTAG21X_OK) return status;
    return setAuthLimit(0);
}

//============================================================================
// Lock (IRREVERSIBILI)
//   pagine 3..15  : lock statici, pagina 2 byte 2-3 (datasheet 8.5.2)
//   pagine 16..   : lock dinamici (8.5.3), granularità 2 pagine (NTAG213) o
//                   16 pagine (NTAG215/216)
//   block-lock    : congelano i lock bit di un'area
//============================================================================
bool RC522_NTAG21x::lockBitFor(uint8_t page, bool block, uint8_t& byteIndex,
                               uint8_t& mask, bool& dynamic) const {
    if (page >= 3 && page <= 15) {
        dynamic = false;
        if (!block) {
            byteIndex = page < 8 ? 0 : 1;
            mask = (uint8_t)(1u << (page < 8 ? page : page - 8));
        } else {
            byteIndex = 0;
            mask = page == 3 ? 0x01 : (page <= 9 ? 0x02 : 0x04);
        }
        return true;
    }
    if (page >= 16 && page <= _info.lastUserPage) {
        dynamic = true;
        const uint8_t granularity = (_info.model == NTAG213) ? 2 : 16;
        const uint8_t group = (page - 16) / granularity;
        if (!block) {
            byteIndex = group / 8;
            mask = (uint8_t)(1u << (group % 8));
        } else {
            byteIndex = 2;
            mask = (uint8_t)(1u << (group / 2));
        }
        return true;
    }
    return false;
}

bool RC522_NTAG21x::isPageLocked(uint8_t page, const NTAG21x_Config& cfg) const {
    uint8_t byteIndex = 0, mask = 0;
    bool dynamic = false;
    if (!lockBitFor(page, false, byteIndex, mask, dynamic)) return false;
    if (dynamic) return cfg.configValid && (cfg.dynamicLock[byteIndex] & mask) != 0;
    return cfg.lockValid && (cfg.staticLock[byteIndex] & mask) != 0;
}

// I lock bit si possono solo impostare. La scrittura sulla pagina 2 viene messa
// in OR dal chip con i lock esistenti; sui lock dinamici i byte RFUI vanno a 0.
NTAG21x_Status RC522_NTAG21x::orLockBytes(bool dynamic, const uint8_t masks[3]) {
    const uint8_t page = dynamic ? _info.dynamicLockPage : 2;
    const uint8_t first = dynamic ? 0 : 2;     // primo byte dei lock nella pagina
    const uint8_t count = dynamic ? 3 : 2;

    uint8_t current[NTAG21x_Protocol::PAGE_SIZE];
    NTAG21x_Status status = readPage(page, current);
    if (status != NTAG21X_OK) return status;

    uint8_t next[NTAG21x_Protocol::PAGE_SIZE] = {0, 0, 0, 0};
    bool changed = false;
    for (uint8_t i = 0; i < count; ++i) {
        next[first + i] = current[first + i] | masks[i];
        if (next[first + i] != current[first + i]) changed = true;
    }
    if (!changed) return NTAG21X_OK;          // già bloccato

    status = writePage(page, next);
    if (status != NTAG21X_OK) return status;

    status = readPage(page, current);
    if (status != NTAG21X_OK) return status;
    for (uint8_t i = 0; i < count; ++i)
        if ((current[first + i] & masks[i]) != masks[i]) return NTAG21X_VERIFY_FAILED;
    return NTAG21X_OK;
}

NTAG21x_Status RC522_NTAG21x::setLockBit(uint8_t page, bool block, bool confirm) {
    if (!confirm) return NTAG21X_INVALID_ARGUMENT;
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;

    uint8_t byteIndex = 0, mask = 0;
    bool dynamic = false;
    if (!lockBitFor(page, block, byteIndex, mask, dynamic)) return NTAG21X_INVALID_ARGUMENT;

    uint8_t masks[3] = {0, 0, 0};
    masks[byteIndex] = mask;
    return orLockBytes(dynamic, masks);
}

// Blocca in sola lettura la pagina indicata (3..lastUserPage). Con i lock
// dinamici blocca l'intero gruppo (2 pagine su NTAG213, 16 su NTAG215/216).
NTAG21x_Status RC522_NTAG21x::lockPage(uint8_t page, bool confirm) {
    return setLockBit(page, false, confirm);
}

// Congela i lock bit dell'area che contiene la pagina: non potranno più essere
// impostati né (ovviamente) rimossi.
NTAG21x_Status RC522_NTAG21x::freezeLockBits(uint8_t page, bool confirm) {
    return setLockBit(page, true, confirm);
}

// Tag in sola lettura per sempre: accesso CC in scrittura "negato" (NFC Forum,
// solo se il tag è formattato NDEF) + lock statici 3..15 + tutti i lock dinamici.
// I block-lock non vengono impostati: i lock restano "congelabili" a parte.
NTAG21x_Status RC522_NTAG21x::makeReadOnly(bool confirm) {
    if (!confirm) return NTAG21X_INVALID_ARGUMENT;
    if (!_initialized) return NTAG21X_NOT_INITIALIZED;

    uint8_t cc[NTAG21x_Protocol::PAGE_SIZE];
    NTAG21x_Status status = readPage(3, cc);
    if (status != NTAG21X_OK) return status;
    if (cc[0] == 0xE1 && (cc[3] & 0x0F) != 0x0F) {
        const uint8_t ccWrite[4] = {0x00, 0x00, 0x00, 0x0F};   // messo in OR dal chip
        status = writePage(3, ccWrite);
        if (status != NTAG21X_OK) return status;
        status = readPage(3, cc);
        if (status != NTAG21X_OK) return status;
        if ((cc[3] & 0x0F) != 0x0F) return NTAG21X_VERIFY_FAILED;
    }

    const uint8_t staticMasks[3] = {0xF8, 0xFF, 0x00};         // L3..L7, L8..L15
    status = orLockBytes(false, staticMasks);
    if (status != NTAG21X_OK) return status;

    const uint8_t granularity = (_info.model == NTAG213) ? 2 : 16;
    const uint8_t groups = (uint8_t)((_info.lastUserPage - 16) / granularity + 1);
    uint8_t dynamicMasks[3] = {0, 0, 0};
    for (uint8_t g = 0; g < groups; ++g)
        dynamicMasks[g / 8] |= (uint8_t)(1u << (g % 8));
    return orLockBytes(true, dynamicMasks);
}

//----------------------------------------------------------------------------
// Interpretazione della configurazione
//----------------------------------------------------------------------------
static const __FlashStringHelper* ccAccessText(uint8_t nibble) {
    if (nibble == 0x0) return F("libera");
    if (nibble == 0xF) return F("negata");
    return F("riservata");
}

static bool dynamicBit(const NTAG21x_Config& cfg, uint8_t group) {
    return ((cfg.dynamicLock[group / 8] >> (group % 8)) & 1) != 0;
}

static void printRange(Print& out, bool& first, uint8_t from, uint8_t to) {
    if (!first) out.print(F(", "));
    first = false;
    out.print(from);
    if (to != from) { out.print('-'); out.print(to); }
}

NTAG21x_Status RC522_NTAG21x::interpretConfig(Print& out) {
    NTAG21x_Config cfg;
    NTAG21x_Status status = readConfig(cfg);
    // Stampa comunque quanto è stato letto (la lettura può essere parziale).
    interpretConfig(out, cfg);
    return status;
}

void RC522_NTAG21x::interpretConfig(Print& out, const NTAG21x_Config& cfg) const {
    if (!cfg.lockValid) {
        out.println(F("Interpretazione impossibile: configurazione non letta"));
        return;
    }
    out.println(F("=== Interpretazione configurazione ==="));

    // ---------------- NDEF / Capability Container ----------------
    const uint8_t* cc = cfg.capabilityContainer;
    out.print(F("[NDEF] "));
    if (cc[0] == 0xE1) {
        out.print(F("Type 2 v")); out.print(cc[1] >> 4); out.print('.'); out.print(cc[1] & 0x0F);
        out.print(F(", area dati ")); out.print((unsigned)cc[2] * 8);
        out.print(F(" byte, lettura ")); out.print(ccAccessText(cc[3] >> 4));
        out.print(F(", scrittura ")); out.print(ccAccessText(cc[3] & 0x0F));
        if (cfg.configValid && cfg.auth0 <= _info.lastPage && (cc[3] & 0x0F) == 0)
            out.print(F(" (ma AUTH0 richiede password, vedi [ACCESSO])"));
        out.println();
    } else if (cc[0] == 0x00) {
        out.println(F("capability container vuoto: tag non formattato NDEF"));
    } else {
        out.println(F("capability container non standard"));
    }

    // ---------------- Lock statici ----------------
    out.print(F("[LOCK] static: "));
    bool any = false;
    for (uint8_t p = 3; p <= 15; ++p) {
        if (cfg.isStaticPageLocked(p)) {
            if (any) out.print(',');
            out.print(p);
            any = true;
        }
    }
    out.println(any ? F(" in sola lettura (permanente)") : F("nessuna pagina bloccata"));
    if (cfg.staticLock[0] & 0x07) {
        out.print(F("       lock bit congelati (non piu' modificabili): "));
        bool first = true;
        if (cfg.staticLock[0] & 0x01) printRange(out, first, 3, 3);
        if (cfg.staticLock[0] & 0x02) printRange(out, first, 4, 9);
        if (cfg.staticLock[0] & 0x04) printRange(out, first, 10, 15);
        out.println();
    }

    if (!cfg.configValid) {
        out.println(F("[ACCESSO] CFG0/CFG1 e lock dinamici non leggibili: lettura protetta"));
        out.println(F("          (PROT=1 con AUTH0 attivo). Eseguire authenticate() e riprovare."));
        return;
    }

    // ---------------- Lock dinamici ----------------
    const uint8_t granularity = (_info.model == NTAG213) ? 2 : 16;
    const uint8_t groups = (uint8_t)((_info.lastUserPage - 16) / granularity + 1);
    out.print(F("       dinamici: "));
    bool firstDyn = true;
    uint8_t g = 0;
    while (g < groups) {
        if (!dynamicBit(cfg, g)) { ++g; continue; }
        uint8_t h = g;
        while ((uint8_t)(h + 1) < groups && dynamicBit(cfg, h + 1)) ++h;
        uint16_t lastP = 16 + (uint16_t)(h + 1) * granularity - 1;
        if (lastP > _info.lastUserPage) lastP = _info.lastUserPage;
        printRange(out, firstDyn, (uint8_t)(16 + g * granularity), (uint8_t)lastP);
        g = h + 1;
    }
    out.println(firstDyn ? F("nessuna pagina bloccata") : F(" in sola lettura (permanente)"));
    if (cfg.dynamicLock[2] != 0) {
        out.print(F("       lock dinamici congelati: "));
        bool firstBl = true;
        for (uint8_t b = 0; b <= (groups - 1) / 2; ++b) {
            if (!(cfg.dynamicLock[2] & (1u << b))) continue;
            uint16_t a = 16 + (uint16_t)(2 * b) * granularity;
            uint16_t z = a + 2 * granularity - 1;
            if (z > _info.lastUserPage) z = _info.lastUserPage;
            printRange(out, firstBl, (uint8_t)a, (uint8_t)z);
        }
        out.println();
    }

    // ---------------- Configurazione di fabbrica? ----------------
    // CFG1 byte 1 è RFUI nel datasheet ma i tag reali escono con 0x05.
    static const uint8_t factoryCfg0[4] = { 0x04, 0x00, 0x00, 0xFF };
    static const uint8_t factoryCfg1[4] = { 0x00, 0x05, 0x00, 0x00 };
    const bool factory = memcmp(cfg.cfg0, factoryCfg0, 4) == 0 &&
                         memcmp(cfg.cfg1, factoryCfg1, 4) == 0;
    out.print(F("[STATO] "));
    out.println(factory ? F("configurazione identica a quella di fabbrica")
                        : F("configurazione modificata rispetto alla fabbrica"));
    if (cfg.configLocked) {
        out.println(F("[STATO] CFGLCK attivo: CFG0 e CFG1 non piu' modificabili (irreversibile)."));
        out.println(F("        PWD e PACK restano scrivibili: proteggerli con AUTH0."));
    }

    // ---------------- Accesso / password ----------------
    out.print(F("[ACCESSO] "));
    const bool protectionOn = cfg.auth0 <= _info.lastPage;
    if (!protectionOn) {
        out.println(F("nessuna pagina protetta da password (AUTH0 oltre l'ultima pagina)"));
        if (cfg.readProtected)
            out.println(F("          PROT=1 ma senza effetto finche' AUTH0 non viene impostato"));
    } else {
        out.print(F("dalla pagina ")); out.print(cfg.auth0); out.print(F(" alla ")); out.print(_info.lastPage);
        out.print(F(" serve PWD_AUTH per la scrittura"));
        out.println(cfg.readProtected ? F(" E per la lettura") : F(" (lettura libera)"));
        if (cfg.auth0 <= 3)
            out.println(F("          l'area protetta include il capability container"));
        if (cfg.auth0 > _info.lastUserPage) {
            out.println(F("          nessuna pagina utente protetta"));
            if (cfg.auth0 == _info.pwdPage)
                out.println(F("          protetti solo PWD e PACK (impostazione consigliata dal datasheet)"));
        } else if (cfg.auth0 > _info.firstUserPage) {
            out.print(F("          pagine utente ")); out.print(_info.firstUserPage);
            out.print('-'); out.print(cfg.auth0 - 1); out.println(F(" sempre libere"));
        }
        if (cfg.auth0 <= _info.configStartPage + 1)
            out.println(F("          anche le pagine di configurazione (CFG0/CFG1) sono protette"));
        out.println(F("          PWD/PACK non sono leggibili: non si sa se valgono ancora FFFFFFFF / 0000"));
    }
    out.print(F("          tentativi password errati: "));
    if (cfg.authLimit == 0) {
        out.println(F("illimitati"));
    } else {
        out.print(F("max ")); out.print(cfg.authLimit);
        out.println(F(": al limite la memoria protetta si blocca per sempre, anche con la password giusta"));
        out.println(F("          (un'autenticazione riuscita azzera il conteggio)"));
    }

    // ---------------- Contatore NFC ----------------
    out.print(F("[CONTATORE] "));
    if (!cfg.nfcCounterEnabled) {
        out.println(F("disattivato"));
    } else {
        out.print(F("attivo (incrementa alla prima lettura dopo l'alimentazione, si ferma a FFFFFF)"));
        out.println(cfg.nfcCounterPwdProtected ? F(", READ_CNT e mirror solo dopo password")
                                               : F(", READ_CNT libero"));
    }

    // ---------------- ASCII mirror ----------------
    out.print(F("[MIRROR] "));
    if (cfg.mirrorMode == NTAG21X_MIRROR_OFF) {
        out.println(F("disattivato"));
    } else {
        uint8_t length = NTAG21x_Cfg::MIRROR_LEN_UID;
        if (cfg.mirrorMode == NTAG21X_MIRROR_COUNTER) length = NTAG21x_Cfg::MIRROR_LEN_COUNTER;
        if (cfg.mirrorMode == NTAG21X_MIRROR_UID_COUNTER) length = NTAG21x_Cfg::MIRROR_LEN_BOTH;

        if (cfg.mirrorMode == NTAG21X_MIRROR_UID) out.print(F("UID"));
        else if (cfg.mirrorMode == NTAG21X_MIRROR_COUNTER) out.print(F("contatore NFC"));
        else out.print(F("UID + 'x' + contatore NFC"));
        out.print(F(" in ASCII ("));
        out.print(length);
        out.print(F(" caratteri) dalla pagina ")); out.print(cfg.mirrorPage);
        out.print(F(" byte ")); out.println(cfg.mirrorByte);

        // Il datasheet (tab. 13/16/19) ammette il mirror solo se finisce prima
        // dell'ultimo byte della memoria utente.
        const uint16_t start = (uint16_t)cfg.mirrorPage * NTAG21x_Memory::PAGE_SIZE + cfg.mirrorByte;
        const uint16_t end = start + length - 1;
        const uint16_t limit = ((uint16_t)_info.lastUserPage + 1) * NTAG21x_Memory::PAGE_SIZE;
        if (cfg.mirrorPage < NTAG21x_Memory::PAGE_USER_START || end + 1 >= limit) {
            out.println(F("          ATTENZIONE: posizione fuori dalla memoria utente, il mirror NON viene applicato"));
        } else {
            out.print(F("          occupa fino a pagina ")); out.print(end / NTAG21x_Memory::PAGE_SIZE);
            out.print(F(" byte ")); out.print(end % NTAG21x_Memory::PAGE_SIZE);
            out.println(F(" (visibile in lettura, non scritto in memoria)"));
        }
        if ((cfg.mirrorMode == NTAG21X_MIRROR_COUNTER || cfg.mirrorMode == NTAG21X_MIRROR_UID_COUNTER) &&
            !cfg.nfcCounterEnabled)
            out.println(F("          il contatore resta 000000 finche' il contatore NFC e' disattivato"));
    }

    // ---------------- Modulazione ----------------
    out.print(F("[RF] strong modulation "));
    out.println(cfg.strongModulation ? F("attiva (valore di fabbrica)") : F("disattiva"));
}
