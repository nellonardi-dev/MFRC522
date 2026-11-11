#ifndef RC522_ISO14443A_H
#define RC522_ISO14443A_H

#include <Arduino.h>
#include "RC522_Driver.h"

// Profilo RF ISO/IEC 14443-A, 106 kbit/s.
namespace RC522_ISO14443A_Config {
    constexpr uint8_t TX_MODE = 0x00;
    constexpr uint8_t RX_MODE = 0x00;
    constexpr uint8_t TX_ASK = 0x40;
    constexpr uint8_t MOD_WIDTH = 0x26;
    constexpr uint8_t MODE_REG = 0x3D;
    constexpr RC522_Driver::BaudRate BAUD_RATE = RC522_Driver::BAUD_106KBPS;
}

enum PICC_Command : uint8_t {
    PICC_CMD_REQA    = 0x26,
    PICC_CMD_WUPA    = 0x52,
    PICC_CMD_CT      = 0x88,
    PICC_CMD_SEL_CL1 = 0x93,
    PICC_CMD_SEL_CL2 = 0x95,
    PICC_CMD_SEL_CL3 = 0x97,
    PICC_CMD_HLTA    = 0x50
};

enum ISO14443A_Status : uint8_t {
    ISO14443A_OK = 0,
    ISO14443A_NO_CARD,
    ISO14443A_COLLISION,
    ISO14443A_TIMEOUT,
    ISO14443A_CRC_ERROR,
    ISO14443A_PROTOCOL_ERROR,
    ISO14443A_INVALID_UID,
    ISO14443A_ERROR
};

struct ISO14443A_UID {
    uint8_t bytes[10];
    uint8_t size;
    uint8_t sak;       // SAK dell'ultimo livello di cascata
};

struct ISO14443A_CardInfo {
    ISO14443A_UID uid;
    uint8_t atqa[2];
    bool valid;
};

class RC522_ISO14443A {
private:
    RC522_Driver* _driver;
    ISO14443A_UID _uid;
    uint8_t _atqa[2];
    bool _initialized;

    void configureISO14443A();
    ISO14443A_Status convertStatus(StatusCode status);

    

    ISO14443A_Status anticollisionLevel(uint8_t cascadeCommand, uint8_t* cascadeData);
    ISO14443A_Status selectLevel(uint8_t cascadeCommand, uint8_t* cascadeData, uint8_t* sak);
    ISO14443A_Status resolveCascade(
        uint8_t cascadeCommand,
        uint8_t* uidData,
        uint8_t& uidLength,
        bool& cascade
    );

public:
    explicit RC522_ISO14443A(RC522_Driver& driver);

    bool begin();

    ISO14443A_Status request();
    ISO14443A_Status wakeup();
    ISO14443A_Status anticollision();
    ISO14443A_Status selectCard();
    ISO14443A_Status readCardSerial();
    ISO14443A_Status halt();

    // --------------------------------------------------
    // Trasporto ISO14443-A generico
    // --------------------------------------------------
    ISO14443A_Status communicate(
        const uint8_t* sendData,
        uint8_t sendLen,
        uint8_t* recvData,
        uint8_t* recvLen,
        uint8_t recvCapacity,
        uint8_t txLastBits = 0,
        uint8_t rxAlign = 0,
        uint8_t* rxLastBits = nullptr,
        uint8_t* collisionPos = nullptr,
        bool checkCRC = false
    );

    // Calcolo CRC_A secondo ISO14443-A.
    ISO14443A_Status calculateCRC_A(
        const uint8_t* data,
        uint8_t length,
        uint8_t* result
    );

    const ISO14443A_UID& getUID() const;
    const uint8_t* getATQA() const;
    uint8_t getSAK() const;
    ISO14443A_CardInfo getCardInfo() const;

    bool isInitialized() const;

    // clearUID conserva ATQA; clearCard azzera tutte le informazioni.
    void clearUID();
    void clearCard();
};

#endif