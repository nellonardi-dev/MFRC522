/*
#ifndef RC522_DRIVER_H
#define RC522_DRIVER_H

#include <Arduino.h>
#include <SPI.h>

// Firmware data for self-test
// Reference values based on firmware version
// Hint: if needed, you can remove unused self-test data to save flash memory
//
// Version 0.0 (0x90)
// Philips Semiconductors; Preliminary Specification Revision 2.0 - 01 August 2005; 16.1 self-test
const byte MFRC522_firmware_referenceV0_0[] PROGMEM = {
	0x00, 0x87, 0x98, 0x0f, 0x49, 0xFF, 0x07, 0x19,
	0xBF, 0x22, 0x30, 0x49, 0x59, 0x63, 0xAD, 0xCA,
	0x7F, 0xE3, 0x4E, 0x03, 0x5C, 0x4E, 0x49, 0x50,
	0x47, 0x9A, 0x37, 0x61, 0xE7, 0xE2, 0xC6, 0x2E,
	0x75, 0x5A, 0xED, 0x04, 0x3D, 0x02, 0x4B, 0x78,
	0x32, 0xFF, 0x58, 0x3B, 0x7C, 0xE9, 0x00, 0x94,
	0xB4, 0x4A, 0x59, 0x5B, 0xFD, 0xC9, 0x29, 0xDF,
	0x35, 0x96, 0x98, 0x9E, 0x4F, 0x30, 0x32, 0x8D
};
// Version 1.0 (0x91)
// NXP Semiconductors; Rev. 3.8 - 17 September 2014; 16.1.1 self-test
const byte MFRC522_firmware_referenceV1_0[] PROGMEM = {
	0x00, 0xC6, 0x37, 0xD5, 0x32, 0xB7, 0x57, 0x5C,
	0xC2, 0xD8, 0x7C, 0x4D, 0xD9, 0x70, 0xC7, 0x73,
	0x10, 0xE6, 0xD2, 0xAA, 0x5E, 0xA1, 0x3E, 0x5A,
	0x14, 0xAF, 0x30, 0x61, 0xC9, 0x70, 0xDB, 0x2E,
	0x64, 0x22, 0x72, 0xB5, 0xBD, 0x65, 0xF4, 0xEC,
	0x22, 0xBC, 0xD3, 0x72, 0x35, 0xCD, 0xAA, 0x41,
	0x1F, 0xA7, 0xF3, 0x53, 0x14, 0xDE, 0x7E, 0x02,
	0xD9, 0x0F, 0xB5, 0x5E, 0x25, 0x1D, 0x29, 0x79
};
// Version 2.0 (0x92)
// NXP Semiconductors; Rev. 3.8 - 17 September 2014; 16.1.1 self-test
const byte MFRC522_firmware_referenceV2_0[] PROGMEM = {
	0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
	0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
	0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
	0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
	0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
	0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
	0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
	0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F
};
// Clone
// Fudan Semiconductor FM17522 (0x88)
const byte FM17522_firmware_reference[] PROGMEM = {
	0x00, 0xD6, 0x78, 0x8C, 0xE2, 0xAA, 0x0C, 0x18,
	0x2A, 0xB8, 0x7A, 0x7F, 0xD3, 0x6A, 0xCF, 0x0B,
	0xB1, 0x37, 0x63, 0x4B, 0x69, 0xAE, 0x91, 0xC7,
	0xC3, 0x97, 0xAE, 0x77, 0xF4, 0x37, 0xD7, 0x9B,
	0x7C, 0xF5, 0x3C, 0x11, 0x8F, 0x15, 0xC3, 0xD7,
	0xC1, 0x5B, 0x00, 0x2A, 0xD0, 0x75, 0xDE, 0x9E,
	0x51, 0x64, 0xAB, 0x3E, 0xE9, 0x15, 0xB5, 0xAB,
	0x56, 0x9A, 0x98, 0x82, 0x26, 0xEA, 0x2A, 0x62
};

enum RC522_Driver_Register : uint8_t {
	// Page 0: Command and status
	//						  0x00			// reserved for future use
	CommandReg				= 0x01 << 1,	// starts and stops command execution
	ComIEnReg				= 0x02 << 1,	// enable and disable interrupt request control bits
	DivIEnReg				= 0x03 << 1,	// enable and disable interrupt request control bits
	ComIrqReg				= 0x04 << 1,	// interrupt request bits
	DivIrqReg				= 0x05 << 1,	// interrupt request bits
	ErrorReg				= 0x06 << 1,	// error bits showing the error status of the last command executed 
	Status1Reg				= 0x07 << 1,	// communication status bits
	Status2Reg				= 0x08 << 1,	// receiver and transmitter status bits
	FIFODataReg				= 0x09 << 1,	// input and output of 64 byte FIFO buffer
	FIFOLevelReg			= 0x0A << 1,	// number of bytes stored in the FIFO buffer
	WaterLevelReg			= 0x0B << 1,	// level for FIFO underflow and overflow warning
	ControlReg				= 0x0C << 1,	// miscellaneous control registers
	BitFramingReg			= 0x0D << 1,	// adjustments for bit-oriented frames
	CollReg					= 0x0E << 1,	// bit position of the first bit-collision detected on the RF interface
	//						  0x0F			// reserved for future use
	
	// Page 1: Command
	// 						  0x10			// reserved for future use
	ModeReg					= 0x11 << 1,	// defines general modes for transmitting and receiving 
	TxModeReg				= 0x12 << 1,	// defines transmission data rate and framing
	RxModeReg				= 0x13 << 1,	// defines reception data rate and framing
	TxControlReg			= 0x14 << 1,	// controls the logical behavior of the antenna driver pins TX1 and TX2
	TxASKReg				= 0x15 << 1,	// controls the setting of the transmission modulation
	TxSelReg				= 0x16 << 1,	// selects the internal sources for the antenna driver
	RxSelReg				= 0x17 << 1,	// selects internal receiver settings
	RxThresholdReg			= 0x18 << 1,	// selects thresholds for the bit decoder
	DemodReg				= 0x19 << 1,	// defines demodulator settings
	// 						  0x1A			// reserved for future use
	// 						  0x1B			// reserved for future use
	MfTxReg					= 0x1C << 1,	// controls some MIFARE communication transmit parameters
	MfRxReg					= 0x1D << 1,	// controls some MIFARE communication receive parameters
	// 						  0x1E			// reserved for future use
	SerialSpeedReg			= 0x1F << 1,	// selects the speed of the serial UART interface
	
	// Page 2: Configuration
	// 						  0x20			// reserved for future use
	CRCResultRegH			= 0x21 << 1,	// shows the MSB and LSB values of the CRC calculation
	CRCResultRegL			= 0x22 << 1,
	// 						  0x23			// reserved for future use
	ModWidthReg				= 0x24 << 1,	// controls the ModWidth setting?
	// 						  0x25			// reserved for future use
    RFCfgReg				= 0x26 << 1,	// configures the receiver gain
	GsNReg					= 0x27 << 1,	// selects the conductance of the antenna driver pins TX1 and TX2 for modulation 
	CWGsPReg				= 0x28 << 1,	// defines the conductance of the p-driver output during periods of no modulation
	ModGsPReg				= 0x29 << 1,	// defines the conductance of the p-driver output during periods of modulation
	TModeReg				= 0x2A << 1,	// defines settings for the internal timer
	TPrescalerReg			= 0x2B << 1,	// the lower 8 bits of the TPrescaler value. The 4 high bits are in TModeReg.
	TReloadRegH				= 0x2C << 1,	// defines the 16-bit timer reload value
	TReloadRegL				= 0x2D << 1,
	TCounterValueRegH		= 0x2E << 1,	// shows the 16-bit timer value
	TCounterValueRegL		= 0x2F << 1,
	
	// Page 3: Test Registers
	// 						  0x30			// reserved for future use
	TestSel1Reg				= 0x31 << 1,	// general test signal configuration
	TestSel2Reg				= 0x32 << 1,	// general test signal configuration
	TestPinEnReg			= 0x33 << 1,	// enables pin output driver on pins D1 to D7
	TestPinValueReg			= 0x34 << 1,	// defines the values for D1 to D7 when it is used as an I/O bus
	TestBusReg				= 0x35 << 1,	// shows the status of the internal test bus
	AutoTestReg				= 0x36 << 1,	// controls the digital self-test
	VersionReg				= 0x37 << 1,	// shows the software version
	AnalogTestReg			= 0x38 << 1,	// controls the pins AUX1 and AUX2
	TestDAC1Reg				= 0x39 << 1,	// defines the test value for TestDAC1
	TestDAC2Reg				= 0x3A << 1,	// defines the test value for TestDAC2
	TestADCReg				= 0x3B << 1		// shows the value of ADC I and Q channels
	// 						  0x3C			// reserved for production tests
	// 						  0x3D			// reserved for production tests
	// 						  0x3E			// reserved for production tests
	// 						  0x3F			// reserved for production tests
};

	// MFRC522 commands. Described in chapter 10 of the datasheet.
enum RC522_Driver_Command : uint8_t {
		PCD_Idle				= 0x00,		// no action, cancels current command execution
		PCD_Mem					= 0x01,		// stores 25 bytes into the internal buffer
		PCD_GenerateRandomID	= 0x02,		// generates a 10-byte random ID number
		PCD_CalcCRC				= 0x03,		// activates the CRC coprocessor or performs a self-test
		PCD_Transmit			= 0x04,		// transmits data from the FIFO buffer
		PCD_NoCmdChange			= 0x07,		// no command change, can be used to modify the CommandReg register bits without affecting the command, for example, the PowerDown bit
		PCD_Receive				= 0x08,		// activates the receiver circuits
		PCD_Transceive 			= 0x0C,		// transmits data from FIFO buffer to antenna and automatically activates the receiver after transmission
		PCD_MFAuthent 			= 0x0E,		// performs the MIFARE standard authentication as a reader
		PCD_SoftReset			= 0x0F		// resets the MFRC522
};

// MFRC522 RxGain[2:0] masks, defines the receiver's signal voltage gain factor (on the PCD).
// Described in 9.3.3.6 / table 98 of the datasheet at http://www.nxp.com/documents/data_sheet/MFRC522.pdf
enum PCD_RxGain : byte {
	RxGain_18dB				= 0x00 << 4,	// 000b - 18 dB, minimum
	RxGain_23dB				= 0x01 << 4,	// 001b - 23 dB
	RxGain_18dB_2			= 0x02 << 4,	// 010b - 18 dB, it seems 010b is a duplicate for 000b
	RxGain_23dB_2			= 0x03 << 4,	// 011b - 23 dB, it seems 011b is a duplicate for 001b
	RxGain_33dB				= 0x04 << 4,	// 100b - 33 dB, average, and typical default
	RxGain_38dB				= 0x05 << 4,	// 101b - 38 dB
	RxGain_43dB				= 0x06 << 4,	// 110b - 43 dB
	RxGain_48dB				= 0x07 << 4,	// 111b - 48 dB, maximum
	RxGain_min				= 0x00 << 4,	// 000b - 18 dB, minimum, convenience for RxGain_18dB
	RxGain_avg				= 0x04 << 4,	// 100b - 33 dB, average, convenience for RxGain_33dB
	RxGain_max				= 0x07 << 4		// 111b - 48 dB, maximum, convenience for RxGain_48dB
};

enum StatusCode : uint8_t {
    STATUS_OK      = 0x01,
    STATUS_TIMEOUT = 0x02,
    STATUS_ERROR   = 0x03,
    STATUS_COLLISION,
    STATUS_NO_ROOM
};

enum PICC_Command : byte {
	// The commands used by the PCD to manage communication with several PICCs (ISO 14443-3, Type A, section 6.4)
	PICC_CMD_REQA			= 0x26,		// REQuest command, Type A. Invites PICCs in state IDLE to go to READY and prepare for anticollision or selection. 7 bit frame.
	PICC_CMD_WUPA			= 0x52,		// Wake-UP command, Type A. Invites PICCs in state IDLE and HALT to go to READY(*) and prepare for anticollision or selection. 7 bit frame.
	PICC_CMD_CT				= 0x88,		// Cascade Tag. Not really a command, but used during anti collision.
	PICC_CMD_SEL_CL1		= 0x93,		// Anti collision/Select, Cascade Level 1
	PICC_CMD_SEL_CL2		= 0x95,		// Anti collision/Select, Cascade Level 2
	PICC_CMD_SEL_CL3		= 0x97,		// Anti collision/Select, Cascade Level 3
	PICC_CMD_HLTA			= 0x50,		// HaLT command, Type A. Instructs an ACTIVE PICC to go to state HALT.
	PICC_CMD_RATS           = 0xE0,     // Request command for Answer To Reset.
};

class RC522_Driver {
private:
    uint8_t _ssPin;
    uint8_t _resetPin;
    SPIClass* _spi;

    void writeRegister(uint8_t reg, uint8_t value);
    void writeRegisterMultiple(uint8_t reg, const uint8_t* values, uint8_t count);
    uint8_t readRegister(uint8_t reg);
    void readRegisterMultiple(uint8_t reg, uint8_t* values, uint8_t count);

    void writeFIFO(const uint8_t* data, uint8_t length);
    void readFIFO(uint8_t* data, uint8_t* length);

    void configureTimer();
    
public:
    RC522_Driver(
        uint8_t ssPin, 
        uint8_t resetPin = 255, 
        SPIClass &spi = SPI
    );
    // --- Configurazione RF dinamica (usata dai protocolli) ---
    enum BaudRate : uint8_t {
        BAUD_106KBPS = 0x00,  // ISO 14443-A default
        BAUD_212KBPS = 0x01,  // FeliCa, ISO 14443-A (dopo PPS)
        BAUD_424KBPS = 0x02,  // FeliCa, ISO 14443-A (dopo PPS)
        BAUD_848KBPS = 0x03   // ISO 14443-A (dopo PPS)
    };
    void setBaudRate(BaudRate rate);
    void setModulation(bool ask100); // true = ASK 100%, false = ASK variabile
    void setCRCMode(uint8_t preset); // 0x3D = ISO 14443, altri valori per standard diversi

    void configureRF( //configurazione per protocollo ISO/IEC 14443A
        uint8_t txMode = 0x00,      // 106kbps  ISO/IEC 14443A
        uint8_t rxMode = 0x00,      // 106kbps
        uint8_t txASK = 0x40,       // ASK 100%
        uint8_t modWidth = 0x26,    // Larghezza modulazione
        uint8_t modeReg = 0x3D      // CRC preset ISO 14443
    );
	void printRFRegisters();
    void begin();
    void reset();
    uint8_t getVersion();
    bool selfTest();

    void setAntenna(bool enabled);
    void setAntennaGain(PCD_RxGain gain);
    PCD_RxGain getAntennaGain(); 

    StatusCode calculateCRC(
        const uint8_t* data,
        uint8_t length,
        uint8_t* result
    );

    StatusCode transceiveData(
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
    uint8_t peekRegister(uint8_t reg); 
   
        
};

#endif

*/

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
    //_spi->transfer((reg >> 1) | 0x80);
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

void RC522_Driver::readFIFO(uint8_t *data, uint8_t *length) {
    *length = readRegister(FIFOLevelReg) & 0x7F; // Bit 7 = FlushBuffer
    readRegisterMultiple(FIFODataReg, data, *length);
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
}
// ============================================================================
// Stampa CONFIGURAZIONE RF
// ============================================================================
void RC522_Driver::printRFRegisters() {

    Serial.println("\n--- RC522 RF REGISTERS ---");

    Serial.printf("TxControlReg : 0x%02X\n",
                  readRegister(TxControlReg));

    Serial.printf("TxASKReg     : 0x%02X\n",
                  readRegister(TxASKReg));

    Serial.printf("RFCfgReg     : 0x%02X\n",
                  readRegister(RFCfgReg));

    Serial.printf("GsNReg       : 0x%02X\n",
                  readRegister(GsNReg));

    Serial.printf("CWGsPReg     : 0x%02X\n",
                  readRegister(CWGsPReg));

    Serial.printf("ModGsPReg    : 0x%02X\n",
                  readRegister(ModGsPReg));

    Serial.println("--------------------------");
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
    uint8_t txMode, rxMode;
    
    switch (rate) {
        case BAUD_106KBPS:
            txMode = 0x00; // 106kbps, TxLength = 0
            rxMode = 0x00;
            break;
        case BAUD_212KBPS:
            txMode = 0x01; // 212kbps
            rxMode = 0x01;
            break;
        case BAUD_424KBPS:
            txMode = 0x02; // 424kbps
            rxMode = 0x02;
            break;
        case BAUD_848KBPS:
            txMode = 0x03; // 848kbps
            rxMode = 0x03;
            break;
        default:
            return; // Non modificare i registri se il valore non è valido
    }
    
    writeRegister(TxModeReg, txMode);
    writeRegister(RxModeReg, rxMode);
}

void RC522_Driver::setModulation(bool ask100) {
    if (ask100) {
        writeRegister(TxASKReg, 0x40); // Forza ASK 100%
    } else {
        writeRegister(TxASKReg, 0x00); // ASK variabile (controllato da ModGsPReg)
    }
}

void RC522_Driver::setCRCMode(uint8_t preset) {
    // ModeReg bit [1:0] = preset value for CRC coprocessor
    // 0x3D = 0b00111101 -> preset = 0x6363 (ISO 14443-3)
    uint8_t currentVal = readRegister(ModeReg);
    writeRegister(ModeReg, (currentVal & ~0x03) | (preset & 0x03));
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
    configureRF();
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
        configureRF();
        setAntenna(true);
        return false;
    }

    // 11. Legge i 64 byte generati dal self-test.
    uint8_t result[64];
    readRegisterMultiple(FIFODataReg, result, sizeof(result));
    Serial.println("Dati self-test letti:");

    for (uint8_t i = 0; i < 64; i++) {
        if (result[i] < 0x10) Serial.print('0');
        Serial.print(result[i], HEX);
        Serial.print(' ');

        if ((i + 1) % 8 == 0) Serial.println();
    }
    // 12. Disabilita il self-test.
    // È necessario per il normale funzionamento del chip.
    writeRegister(AutoTestReg, 0x00);

    // 13. Ripristina il chip e la configurazione operativa.
    // Il self-test può lasciare il dispositivo in uno stato
    // non utilizzabile per la normale comunicazione.
    reset();
    configureTimer();
    configureRF();
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
    if (data == nullptr || result == nullptr || length == 0) {
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
        rxAlign > 7) {
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
            return STATUS_TIMEOUT;
        }
    }

    // Arresta il comando e azzera StartSend.
    writeRegister(CommandReg, PCD_Idle);
    writeRegister(BitFramingReg, 0x00);

    // Timeout software.
    if ((irq & 0x30) == 0) {
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
        // Buffer overflow, CRC, parità o errore protocollo.
        return STATUS_ERROR;
    }

    // --------------------------------------------------
    // 6. Lettura FIFO
    // --------------------------------------------------

    const uint8_t fifoLen = readRegister(FIFOLevelReg) & 0x7F;

    if (fifoLen > 64 || fifoLen > recvCapacity) {
        Serial.println("FIFO OVERFLOW / BUFFER TOO SMALL");
        return STATUS_NO_ROOM;
    }

    if (fifoLen > 0) {
        readRegisterMultiple(FIFODataReg, recvData, fifoLen);
    }

    *recvLen = fifoLen;

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