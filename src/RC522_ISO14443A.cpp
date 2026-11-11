#include "RC522_ISO14443A.h"
#include <string.h>

namespace {
    constexpr uint8_t UID_MAX_LENGTH = 10;
    constexpr uint8_t CASCADE_BIT = 0x04;
    constexpr uint8_t NVB_FULL = 0x70;
}

RC522_ISO14443A::RC522_ISO14443A(RC522_Driver& driver)
    : _driver(&driver), _uid{}, _atqa{}, _initialized(false) {
}

bool RC522_ISO14443A::begin() {
    _initialized = false;
    clearCard();

    if (_driver == nullptr) {
        return false;
    }

    // Il driver originale espone begin() come void.
    _driver->begin();

    // 0x00 e 0xFF indicano normalmente un bus/chip non leggibile.
    // Non imponiamo una whitelist: sono supportati anche cloni con
    // codici versione differenti.
    const uint8_t version = _driver->getVersion();
    if (version == 0x00 || version == 0xFF) {
        return false;
    }

    configureISO14443A();
    _initialized = true;
    return true;
}

void RC522_ISO14443A::configureISO14443A() {
    _driver->configureRF(
        RC522_ISO14443A_Config::TX_MODE,
        RC522_ISO14443A_Config::RX_MODE,
        RC522_ISO14443A_Config::TX_ASK,
        RC522_ISO14443A_Config::MOD_WIDTH,
        RC522_ISO14443A_Config::MODE_REG
    );
    _driver->setBaudRate(RC522_ISO14443A_Config::BAUD_RATE);
}

ISO14443A_Status RC522_ISO14443A::convertStatus(StatusCode status) {
    switch (status) {
        case STATUS_OK:        return ISO14443A_OK;
        case STATUS_COLLISION: return ISO14443A_COLLISION;
        case STATUS_TIMEOUT:   return ISO14443A_TIMEOUT;
        case STATUS_NO_ROOM:   return ISO14443A_ERROR;
        case STATUS_ERROR:
        default:               return ISO14443A_ERROR;
    }
}

ISO14443A_Status RC522_ISO14443A::communicate(
    const uint8_t* sendData,
    uint8_t sendLen,
    uint8_t* recvData,
    uint8_t* recvLen,
    uint8_t recvCapacity,
    uint8_t txLastBits,
    uint8_t rxAlign,
    uint8_t* rxLastBits,
    uint8_t* collisionPos,
    bool checkCRC
) {
    if (!_initialized || _driver == nullptr) return ISO14443A_ERROR;

    return convertStatus(_driver->transceiveData(
        sendData, sendLen, recvData, recvLen, recvCapacity,
        txLastBits, rxAlign, rxLastBits, collisionPos, checkCRC
    ));
}
ISO14443A_Status RC522_ISO14443A::calculateCRC_A(
    const uint8_t* data,
    uint8_t length,
    uint8_t* result
) {
    if (!_initialized || _driver == nullptr) {
        return ISO14443A_ERROR;
    }

    if (data == nullptr || result == nullptr || length == 0) {
        return ISO14443A_ERROR;
    }

    return convertStatus(
        _driver->calculateCRC(data, length, result)
    );
}

ISO14443A_Status RC522_ISO14443A::request() {
    clearCard();

    const uint8_t command = PICC_CMD_REQA;
    uint8_t atqa[2] = {0};
    uint8_t length = 0;
    uint8_t lastBits = 0;

    ISO14443A_Status status = communicate(
        &command, 1, atqa, &length, sizeof(atqa), 7, 0, &lastBits
    );
    if (status != ISO14443A_OK) return status;
    if (length != 2 || lastBits != 0) return ISO14443A_PROTOCOL_ERROR;

    memcpy(_atqa, atqa, sizeof(_atqa));
    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::wakeup() {
    clearCard();

    const uint8_t command = PICC_CMD_WUPA;
    uint8_t atqa[2] = {0};
    uint8_t length = 0;
    uint8_t lastBits = 0;

    ISO14443A_Status status = communicate(
        &command, 1, atqa, &length, sizeof(atqa), 7, 0, &lastBits
    );
    if (status != ISO14443A_OK) return status;
    if (length != 2 || lastBits != 0) return ISO14443A_PROTOCOL_ERROR;

    memcpy(_atqa, atqa, sizeof(_atqa));
    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::anticollisionLevel(
    uint8_t cascadeCommand,
    uint8_t* cascadeData
) {
    if (cascadeData == nullptr) return ISO14443A_ERROR;

    const uint8_t sendData[2] = { cascadeCommand, 0x20 };
    uint8_t buffer[5] = {0};
    uint8_t recvLen = 0;
    uint8_t lastBits = 0;

    ISO14443A_Status status = communicate(
        sendData, sizeof(sendData), buffer, &recvLen, sizeof(buffer),
        0, 0, &lastBits
    );
    if (status != ISO14443A_OK) return status;
    if (recvLen != 5 || lastBits != 0) return ISO14443A_PROTOCOL_ERROR;

    const uint8_t bcc = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3];
    if (bcc != buffer[4]) return ISO14443A_PROTOCOL_ERROR;

    memcpy(cascadeData, buffer, 5);
    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::selectLevel(
    uint8_t cascadeCommand,
    uint8_t* cascadeData,
    uint8_t* sak
) {
    if (cascadeData == nullptr || sak == nullptr) return ISO14443A_ERROR;

    uint8_t sendData[9] = {0};
    sendData[0] = cascadeCommand;
    sendData[1] = NVB_FULL;
    memcpy(&sendData[2], cascadeData, 5);

    uint8_t crc[2] = {0};
    ISO14443A_Status crcStatus = calculateCRC_A(sendData, 7, crc);
    if (crcStatus != ISO14443A_OK) return crcStatus;

    sendData[7] = crc[0];
    sendData[8] = crc[1];

    // Il driver verifica il CRC ricevuto ma mantiene i due byte CRC
    // nel buffer: la risposta SELECT è quindi SAK + CRC_A (3 byte).
    uint8_t response[3] = {0};
    uint8_t responseLength = 0;

    ISO14443A_Status status = communicate(
        sendData, sizeof(sendData), response, &responseLength,
        sizeof(response), 0, 0, nullptr, nullptr, true
    );
    if (status != ISO14443A_OK) return status;
    if (responseLength != 3) return ISO14443A_PROTOCOL_ERROR;

    *sak = response[0];
    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::resolveCascade(
    uint8_t cascadeCommand,
    uint8_t* uidData,
    uint8_t& uidLength,
    bool& cascade
) {
    uint8_t cascadeData[5] = {0};

    ISO14443A_Status status = anticollisionLevel(cascadeCommand, cascadeData);
    if (status != ISO14443A_OK) return status;

    uint8_t sak = 0;
    status = selectLevel(cascadeCommand, cascadeData, &sak);
    if (status != ISO14443A_OK) return status;

    cascade = (sak & CASCADE_BIT) != 0;

    if (cascade) {
        if (cascadeData[0] != PICC_CMD_CT) return ISO14443A_PROTOCOL_ERROR;
        if (uidLength > UID_MAX_LENGTH - 3) return ISO14443A_INVALID_UID;

        memcpy(&uidData[uidLength], &cascadeData[1], 3);
        uidLength += 3;
    } else {
        if (uidLength > UID_MAX_LENGTH - 4) return ISO14443A_INVALID_UID;

        memcpy(&uidData[uidLength], cascadeData, 4);
        uidLength += 4;

        // Il SAK significativo per la carta è quello dell'ultimo
        // livello, cioè quello che non richiede un'altra cascata.
        _uid.sak = sak;
    }

    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::anticollision() {
    if (!_initialized) return ISO14443A_ERROR;

    // Non usare clearCard(): ATQA è stato acquisito da REQA/WUPA
    // e deve rimanere disponibile durante la selezione.
    clearUID();

    uint8_t uidData[UID_MAX_LENGTH] = {0};
    uint8_t uidLength = 0;
    bool cascade = false;

    ISO14443A_Status status = resolveCascade(
        PICC_CMD_SEL_CL1, uidData, uidLength, cascade
    );
    if (status != ISO14443A_OK) {
        clearUID();
        return status;
    }

    if (cascade) {
        status = resolveCascade(
            PICC_CMD_SEL_CL2, uidData, uidLength, cascade
        );
        if (status != ISO14443A_OK) {
            clearUID();
            return status;
        }

        if (cascade) {
            status = resolveCascade(
                PICC_CMD_SEL_CL3, uidData, uidLength, cascade
            );
            if (status != ISO14443A_OK) {
                clearUID();
                return status;
            }
            if (cascade) {
                clearUID();
                return ISO14443A_INVALID_UID;
            }
        }
    }

    if (uidLength != 4 && uidLength != 7 && uidLength != 10) {
        clearUID();
        return ISO14443A_INVALID_UID;
    }

    memcpy(_uid.bytes, uidData, uidLength);
    _uid.size = uidLength;
    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::selectCard() {
    if (!_initialized) return ISO14443A_ERROR;
    return anticollision();
}

ISO14443A_Status RC522_ISO14443A::readCardSerial() {
    ISO14443A_Status status = request();
    if (status != ISO14443A_OK) return status;

    status = selectCard();
    if (status != ISO14443A_OK) return status;

    return ISO14443A_OK;
}

ISO14443A_Status RC522_ISO14443A::halt() {
    if (!_initialized) return ISO14443A_ERROR;

    uint8_t sendData[4] = { PICC_CMD_HLTA, 0x00, 0x00, 0x00 };
    uint8_t crc[2] = {0};

    ISO14443A_Status crcStatus = calculateCRC_A(sendData, 2, crc);
    if (crcStatus != ISO14443A_OK) return crcStatus;

    sendData[2] = crc[0];
    sendData[3] = crc[1];

    uint8_t response[2] = {0};
    uint8_t responseLength = 0;

    ISO14443A_Status status = communicate(
        sendData, sizeof(sendData), response, &responseLength,
        sizeof(response), 0, 0, nullptr, nullptr, false
    );

    // HLTA non prevede una risposta: il timeout è il comportamento atteso.
    if (status == ISO14443A_TIMEOUT) {
        return ISO14443A_OK;
    }
    if (status != ISO14443A_OK) {
        return status;
    }
    return ISO14443A_PROTOCOL_ERROR;

}

const ISO14443A_UID& RC522_ISO14443A::getUID() const {
    return _uid;
}

const uint8_t* RC522_ISO14443A::getATQA() const {
    return _atqa;
}

uint8_t RC522_ISO14443A::getSAK() const {
    return _uid.sak;
}

ISO14443A_CardInfo RC522_ISO14443A::getCardInfo() const {
    ISO14443A_CardInfo info = {};
    info.uid = _uid;
    info.atqa[0] = _atqa[0];
    info.atqa[1] = _atqa[1];
    info.valid = (_uid.size == 4 || _uid.size == 7 || _uid.size == 10);
    return info;
}

bool RC522_ISO14443A::isInitialized() const {
    return _initialized;
}

void RC522_ISO14443A::clearUID() {
    memset(&_uid, 0, sizeof(_uid));
}

void RC522_ISO14443A::clearCard() {
    clearUID();
    memset(_atqa, 0, sizeof(_atqa));
}
