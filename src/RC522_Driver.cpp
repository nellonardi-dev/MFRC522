#include "RC522_Driver.h"

// ============================================================================
// COMUNICAZIONE SPI - SCRITTURA REGISTRO
// ============================================================================
void RC522_Driver::writeRegister(uint8_t reg, uint8_t value) {
    _spi->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi->transfer(reg & 0x7E);
    _spi->transfer(value);
    digitalWrite(_ssPin, HIGH);
    _spi->endTransaction();
}

// ============================================================================
// COMUNICAZIONE SPI - SCRITTURA REGISTRI MULTIPLI
// ============================================================================
void RC522_Driver::writeRegisterMultiple(uint8_t reg, const uint8_t *values, uint8_t count) {
    _spi->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi->transfer(reg & 0x7E);
    for (uint8_t i = 0; i < count; i++) {
        _spi->transfer(values[i]);
    }
    digitalWrite(_ssPin, HIGH);
    _spi->endTransaction();
}

// ============================================================================
// COMUNICAZIONE SPI - LETTURA REGISTRO
// ============================================================================
uint8_t RC522_Driver::readRegister(uint8_t reg) {
    uint8_t value;
    _spi->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi->transfer(reg | 0x80);
    value = _spi->transfer(0x00);
    digitalWrite(_ssPin, HIGH);
    _spi->endTransaction();
    return value;
}

// ============================================================================
// COMUNICAZIONE SPI - LETTURA REGISTRI MULTIPLI
// ============================================================================
void RC522_Driver::readRegisterMultiple(
    uint8_t reg,
    uint8_t *values,
    uint8_t count
) {
    if (count == 0) {
        return;
    }

    const uint8_t address = reg | 0x80;

    _spi->beginTransaction(
        SPISettings(4000000, MSBFIRST, SPI_MODE0)
    );

    digitalWrite(_ssPin, LOW);

    // Seleziona il registro da leggere
    _spi->transfer(address);

    // Per i byte intermedi, ritrasmette l'indirizzo.
    // Per l'ultimo byte, invia 0x00.
    for (uint8_t i = 0; i < count; i++) {
        uint8_t txByte = (i == count - 1) ? 0x00 : address;
        values[i] = _spi->transfer(txByte);
    }

    digitalWrite(_ssPin, HIGH);
    _spi->endTransaction();
}
// ============================================================================
// GESTIONE FIFO
// ============================================================================
void RC522_Driver::writeFIFO(const uint8_t *data, uint8_t length) {
    writeRegisterMultiple(FIFODataReg, data, length);
}

StatusCode RC522_Driver::readFIFO(
    uint8_t* data,
    uint8_t capacity,
    uint8_t* length
) {
    if (data == nullptr || length == nullptr) {
        return STATUS_ERROR;
    }

    *length = 0;

    const uint8_t fifoLength =
        readRegister(FIFOLevelReg) & 0x7F;

    if (fifoLength > 64) {
        return STATUS_ERROR;
    }

    if (fifoLength > capacity) {
        return STATUS_NO_ROOM;
    }

    if (fifoLength > 0) {
        readRegisterMultiple(
            FIFODataReg,
            data,
            fifoLength
        );
    }

    *length = fifoLength;

    return STATUS_OK;
}

// ============================================================================
// RESET (Hard + Soft)
// ============================================================================
void RC522_Driver::reset() {
    // 1. Hard Reset fisico (SOLO se il pin è stato specificato)
    if (_resetPin != 255) {
        pinMode(_resetPin, OUTPUT);
        digitalWrite(_resetPin, LOW);
        delay(10); // 10ms per stabilizzare il power down
        digitalWrite(_resetPin, HIGH);
        delay(50); // 50ms per l'oscillatore e il completamento del reset hardware
    }
    
    // 2. Soft Reset (SEMPRE eseguito per pulire FIFO e macchina a stati interna)
    writeRegister(CommandReg, PCD_SoftReset);
    delay(50); // Attesa necessaria dopo il soft reset
}

// ============================================================================
// CONFIGURAZIONE TIMER
// ============================================================================
void RC522_Driver::configureTimer() {
    // f_timer = 13.56 MHz / (2*TPreScaler+1) 
    writeRegister(TModeReg, 0x80);      // TAuto=1: il timer parte automaticamente alla fine della trasmissione
    writeRegister(TPrescalerReg, 0xA9); // TPreScaler = 169 => f_timer=40kHz (periodo di 25μs)


    writeRegister(TReloadRegH, 0x03);   // Reload 0x3E8 = 1000
    writeRegister(TReloadRegL, 0xE8);   // 1000 * 25μs = 25ms di timeout


    // 2000 periodi da 25 us = 50 ms
//    writeRegister(TReloadRegH, 0x07);  
//    writeRegister(TReloadRegL, 0xD0);
}

// ============================================================================
// CONFIGURAZIONE RF
// ============================================================================
void RC522_Driver::configureRF(uint8_t txMode, uint8_t rxMode, uint8_t txASK, uint8_t modWidth, uint8_t modeReg) {
    writeRegister(TxModeReg, txMode); 
    writeRegister(RxModeReg, rxMode);
    writeRegister(TxASKReg, txASK);  
    writeRegister(ModWidthReg, modWidth );
    writeRegister(ModeReg, modeReg);   
}

void RC522_Driver::setBaudRate(BaudRate rate) {
    const uint8_t speed = static_cast<uint8_t>(rate);

    if (speed > 3) {
        return;
    }

    const uint8_t txMode = readRegister(TxModeReg);
    const uint8_t rxMode = readRegister(RxModeReg);

    // Velocità nei bit [6:4], preservando gli altri bit.
    writeRegister(
        TxModeReg,
        (txMode & 0x8F) | (speed << 4)
    );

    writeRegister(
        RxModeReg,
        (rxMode & 0x8F) | (speed << 4)
    );
}

void RC522_Driver::setModulation(bool ask100) {
    uint8_t value = readRegister(TxASKReg);

    if (ask100) {
        value |= 0x40;
    } else {
        value &= ~0x40;
    }

    writeRegister(TxASKReg, value);
}

void RC522_Driver::setCRCMode(uint8_t preset) {
    // ModeReg bit [1:0] = preset value for CRC coprocessor
    // 0x3D = 0b00111101 -> preset = 0x6363 (ISO 14443-3)
    if (preset > 3) {
        return;
    }

    uint8_t currentVal = readRegister(ModeReg);

    writeRegister(
        ModeReg,
        (currentVal & 0xFC) | preset
    );
}

void RC522_Driver::printRFRegisters() {

    Serial.println(F("----- MFRC522 RF REGISTERS -----"));

    Serial.print(F("TxModeReg: "));
    Serial.println(peekRegister(TxModeReg), HEX);

    Serial.print(F("RxModeReg: "));
    Serial.println(peekRegister(RxModeReg), HEX);

    Serial.print(F("TxASKReg: "));
    Serial.println(peekRegister(TxASKReg), HEX);

    Serial.print(F("ModWidthReg: "));
    Serial.println(peekRegister(ModWidthReg), HEX);

    Serial.print(F("ModeReg: "));
    Serial.println(peekRegister(ModeReg), HEX);

    Serial.println(F("--------------------------------"));
}

// ============================================================================
// GESTIONE ANTENNA
// ============================================================================
void RC522_Driver::setAntenna(bool enabled) {
    uint8_t currentVal = readRegister(TxControlReg);
    
    if (enabled) {
        // Setta i bit 0 e 1 a 1 solo se non sono già settati (risparmia una scrittura SPI)
        if (!(currentVal & 0x03)) {
            writeRegister(TxControlReg, currentVal | 0x03);
        }
    } else {
        // Setta i bit 0 e 1 a 0
        if (currentVal & 0x03) {
            writeRegister(TxControlReg, currentVal & ~0x03);
        }
    }
}

// ============================================================================
// GESTIONE GUADAGNO ANTENNA
// ============================================================================
void RC522_Driver::setAntennaGain(PCD_RxGain gain) {
    uint8_t currentVal = readRegister(RFCfgReg);
    // Pulisci i bit [6:4] e imposta il nuovo guadagno
    writeRegister(RFCfgReg, (currentVal & ~0x70) | gain);
}

PCD_RxGain RC522_Driver::getAntennaGain() {
    return (PCD_RxGain)(readRegister(RFCfgReg) & 0x70);
}

// ============================================================================
// COSTRUTTORE
// ============================================================================
RC522_Driver::RC522_Driver(uint8_t ssPin, uint8_t resetPin,
                       SPIClass &spi)
    : _ssPin(ssPin),
      _resetPin(resetPin),
      _spi(&spi) {

    // Il costruttore forza la definizione del pin SS, risolvendo i bug multimodulo.
    // Il puntatore a SPIClass permette di usare SPI, SPI1, ecc. (utile per ESP32/Pico).
}

// ============================================================================
// INIZIALIZZAZIONE
// ============================================================================
void RC522_Driver::begin() {
    pinMode(_ssPin, OUTPUT);
    digitalWrite(_ssPin, HIGH); // Deseleziona il chip (attivo basso)
    
    _spi->begin();
    
    reset();
    
    configureTimer();
    
    setAntenna(true);
}
// ============================================================================
// Versione del Chip montato sul modulo
// ============================================================================
uint8_t RC522_Driver::getVersion() {
    return readRegister(VersionReg);
}

// ============================================================================
// SELF-TEST (opzionale)
// ============================================================================
bool RC522_Driver::selfTest() {
    // 1. Soft reset
    writeRegister(CommandReg, PCD_SoftReset);
    delay(50);

    // 2. Disabilita l'autotest
    writeRegister(AutoTestReg, 0x00);

    // 3. Arresta il comando corrente
    writeRegister(CommandReg, PCD_Idle);

    // 4. Svuota la FIFO
    writeRegister(FIFOLevelReg, 0x80);

    // 5. Scrive 25 byte a zero nella FIFO
    const uint8_t zeroes[25] = {0};
    writeRegisterMultiple(FIFODataReg, zeroes, sizeof(zeroes));

    // 6. Avvia il trasferimento verso il buffer interno
    writeRegister(CommandReg, PCD_Mem);

    // 7. Abilita il self-test digitale
    writeRegister(AutoTestReg, 0x09);

    // 8. Scrive il byte di avvio
    writeRegister(FIFODataReg, 0x00);

    // 9. Avvia il calcolo CRC
    writeRegister(CommandReg, PCD_CalcCRC);

    // 10. Attende che siano disponibili 64 byte
    // Il datasheet non definisce un interrupt affidabile per questa fase.
    uint8_t fifoLevel = 0;
    const unsigned long timeoutMs = 100;
    const unsigned long startTime = millis();

    while (millis() - startTime < timeoutMs) {
        fifoLevel = readRegister(FIFOLevelReg) & 0x7F;

        if (fifoLevel >= 64) {
            break;
        }

        delay(1);
    }

    writeRegister(CommandReg, PCD_Idle);

    if (fifoLevel < 64) {
        writeRegister(AutoTestReg, 0x00);
        reset();
        
        configureTimer();
        
        setAntenna(true);
        
        return false;
    }

    // 11. Legge i 64 byte generati dal self-test.
    uint8_t result[64];
    readRegisterMultiple(FIFODataReg, result, sizeof(result));

    // 12. Disabilita il self-test.
    // È necessario per il normale funzionamento del chip.
    writeRegister(AutoTestReg, 0x00);

    // 13. Ripristina il chip e la configurazione operativa.
    // Il self-test può lasciare il dispositivo in uno stato
    // non utilizzabile per la normale comunicazione.
    reset();

    configureTimer();
    
    setAntenna(true);

    // 14. Seleziona la sequenza di riferimento in base alla versione.
    const uint8_t version = readRegister(VersionReg);
    const byte* reference = nullptr;

    switch (version) {
        case 0x88:
            reference = FM17522_firmware_reference;
            break;

        case 0x90:
            reference = MFRC522_firmware_referenceV0_0;
            break;

        case 0x91:
            reference = MFRC522_firmware_referenceV1_0;
            break;

        case 0x92:
            reference = MFRC522_firmware_referenceV2_0;
            break;

        default:
            return false;
    }

    // 15. Confronta i 64 byte con i valori ufficiali.
    for (uint8_t i = 0; i < 64; i++) {
        if (result[i] != pgm_read_byte(&reference[i])) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Calcola CRC
// ============================================================================
StatusCode RC522_Driver::calculateCRC(
    const uint8_t* data,
    uint8_t length,
    uint8_t* result
) {
    if (data == nullptr ||
        result == nullptr ||
        length == 0 ||
        length > 64) {
        return STATUS_ERROR;
    }

    // Ferma l'eventuale comando precedente.
    writeRegister(CommandReg, PCD_Idle);

    // Pulisce il flag CRCIRq.
    writeRegister(DivIrqReg, 0x04);

    // Svuota la FIFO.
    writeRegister(FIFOLevelReg, 0x80);

    // Carica i dati per il coprocessore CRC.
    writeFIFO(data, length);

    // Avvia il calcolo CRC.
    writeRegister(CommandReg, PCD_CalcCRC);

    const unsigned long start = millis();

    while ((millis() - start) < 10UL) {
        const uint8_t irq = readRegister(DivIrqReg);

        if (irq & 0x04) {
            // Il calcolo è terminato.
            writeRegister(CommandReg, PCD_Idle);

            result[0] = readRegister(CRCResultRegL);
            result[1] = readRegister(CRCResultRegH);

            return STATUS_OK;
        }
    }

    // Evita di lasciare attivo il coprocessore.
    writeRegister(CommandReg, PCD_Idle);

    return STATUS_TIMEOUT;
}
// ============================================================================
// Funzione TRANSCIEVE (IMPORTANTISSIMA)
// ============================================================================
StatusCode RC522_Driver::transceiveData(
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
    // --------------------------------------------------
    // 1. Validazione parametri
    // --------------------------------------------------
    if (sendData == nullptr ||
        recvData == nullptr ||
        recvLen == nullptr ||
        sendLen == 0 ||
        sendLen > 64 ||
        txLastBits > 7 ||
        rxAlign > 7 ||
        recvCapacity > 64) {
        return STATUS_ERROR;
    }

    *recvLen = 0;

    if (rxLastBits != nullptr) {
        *rxLastBits = 0;
    }

    if (collisionPos != nullptr) {
        *collisionPos = 0;
    }

    // --------------------------------------------------
    // 2. Preparazione MFRC522
    // --------------------------------------------------

    writeRegister(CommandReg, PCD_Idle);

    // Disabilita interrupt e cancella flag pendenti.
    writeRegister(ComIEnReg, 0x00);
    writeRegister(ComIrqReg, 0x7F);

    // Svuota FIFO.
    writeRegister(FIFOLevelReg, 0x80);

    // Configura framing.
    const uint8_t framing =
        static_cast<uint8_t>((rxAlign << 4) | txLastBits);

    writeRegister(BitFramingReg, framing);

    // Carica dati TX.
    writeFIFO(sendData, sendLen);

    // Avvia transceive.
    writeRegister(CommandReg, PCD_Transceive);
    writeRegister(BitFramingReg, framing | 0x80);

    // --------------------------------------------------
    // 3. Attesa completamento operazione
    // --------------------------------------------------

    const unsigned long start = millis();
    uint8_t irq = 0;

    while ((millis() - start) < 30UL) {
        irq = readRegister(ComIrqReg);

        // RxIrq oppure IdleIrq.
        if (irq & 0x30) {
            break;
        }

        // TimerIrq: nessuna risposta.
        if (irq & 0x01) {
            writeRegister(CommandReg, PCD_Idle);
            writeRegister(BitFramingReg, 0x00);
            writeRegister(FIFOLevelReg, 0x80);
            return STATUS_TIMEOUT;
        }
    }

    // Arresta il comando e azzera StartSend.
    writeRegister(CommandReg, PCD_Idle);
    writeRegister(BitFramingReg, 0x00);

    // Timeout software.
    if ((irq & 0x30) == 0) {
        writeRegister(FIFOLevelReg, 0x80);
        return STATUS_TIMEOUT;
    }

    // --------------------------------------------------
    // 4. Lettura registri di stato
    // --------------------------------------------------

    const uint8_t errorReg = readRegister(ErrorReg);
    
    // --------------------------------------------------
    // 5. Gestione collisione
    // --------------------------------------------------

    const bool collision = (errorReg & 0x08) != 0;


    if (collision) {
        const uint8_t collReg = readRegister(CollReg);

        // Posizione collisione: valori da 1 a 32.
        // Zero nel registro indica posizione 32.
        if (collisionPos != nullptr && !(collReg & 0x20)) {
            const uint8_t pos = collReg & 0x1F;
            *collisionPos = (pos == 0) ? 32 : pos;
        }
    }
    else if (errorReg & 0x17) {
        writeRegister(FIFOLevelReg, 0x80);
        writeRegister(BitFramingReg, 0x00);
        return STATUS_ERROR;
    }

    // --------------------------------------------------
    // 6. Lettura FIFO
    // --------------------------------------------------

    const StatusCode fifoStatus =
    readFIFO(recvData, recvCapacity, recvLen);

    if (fifoStatus != STATUS_OK) {
        return fifoStatus;
    }

    const uint8_t fifoLen = *recvLen;

    // Bit validi nell'ultimo byte ricevuto.
    if (rxLastBits != nullptr) {
        *rxLastBits = readRegister(ControlReg) & 0x07;
    }

    // --------------------------------------------------
    // 7. Restituzione collisione
    // --------------------------------------------------

    if (collision) {
        return STATUS_COLLISION;
    }

    // --------------------------------------------------
    // 8. Verifica CRC opzionale
    // --------------------------------------------------

    if (checkCRC) {
        if (fifoLen < 2) {
            return STATUS_ERROR;
        }

        uint8_t crc[2];

        const StatusCode crcStatus =
            calculateCRC(recvData, fifoLen - 2, crc);

        if (crcStatus != STATUS_OK) {
            return crcStatus;
        }

        if (crc[0] != recvData[fifoLen - 2] ||
            crc[1] != recvData[fifoLen - 1]) {
            return STATUS_ERROR;
        }
    }

    // --------------------------------------------------
    // 9. Operazione completata
    // --------------------------------------------------

    return STATUS_OK;
}

uint8_t RC522_Driver::peekRegister(uint8_t reg) { return readRegister(reg); }