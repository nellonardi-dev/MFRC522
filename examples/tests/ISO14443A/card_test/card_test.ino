#include <Arduino.h>
#include "RC522_Driver.h"
#include "RC522_ISO14443A.h"

// Mantieni qui gli stessi pin già utilizzati nel tuo sketch funzionante.
#define SS_PIN  5
#define RST_PIN 26

RC522_Driver driver(SS_PIN, RST_PIN);
RC522_ISO14443A iso(driver);

bool messaggioAttesaStampato = false;

// --------------------------------------------------
// Stampa un byte in esadecimale (sempre due cifre)
// --------------------------------------------------
void printHex(uint8_t value) {
    if (value < 0x10) Serial.print('0');
    Serial.print(value, HEX);
}

// --------------------------------------------------
// Stampa lo stato restituito dalla libreria ISO
// --------------------------------------------------
void printStatus(ISO14443A_Status status) {
    Serial.print(F("Stato ISO: "));

    switch (status) {
        case ISO14443A_OK:
            Serial.println(F("OK"));
            break;

        case ISO14443A_NO_CARD:
            Serial.println(F("nessuna carta"));
            break;

        case ISO14443A_COLLISION:
            Serial.println(F("collisione"));
            break;

        case ISO14443A_TIMEOUT:
            Serial.println(F("timeout"));
            break;

        case ISO14443A_CRC_ERROR:
            Serial.println(F("errore CRC"));
            break;

        case ISO14443A_PROTOCOL_ERROR:
            Serial.println(F("errore protocollo"));
            break;

        case ISO14443A_INVALID_UID:
            Serial.println(F("UID non valido"));
            break;

        case ISO14443A_ERROR:
        default:
            Serial.println(F("errore generico"));
            break;
    }
}

// --------------------------------------------------
// Classificazione PRELIMINARE basata sul SAK.
// Non identifica con certezza il modello della carta.
// --------------------------------------------------
void printProbableFamily(uint8_t sak) {
    Serial.print(F("Famiglia probabile: "));

    switch (sak) {
        case 0x08:
            Serial.println(F("compatibile con MIFARE Classic 1K"));
            break;

        case 0x18:
            Serial.println(F("compatibile con MIFARE Classic 4K"));
            break;

        case 0x00:
            Serial.println(F("MIFARE Ultralight / NTAG o famiglia affine"));
            break;

        case 0x20:
            Serial.println(F("carta con supporto ISO14443-4; DESFire possibile"));
            break;

        default:
            Serial.println(F("non classificata con i soli dati ISO14443A"));
            break;
    }

    Serial.println(
        F("Nota: ATQA e SAK forniscono indizi, non sempre un'identificazione certa.")
    );
}

// --------------------------------------------------
// Stampa ATQA, UID, SAK e classificazione preliminare
// --------------------------------------------------
void printCardInformation() {
    const ISO14443A_CardInfo info = iso.getCardInfo();

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("          CARTA ISO14443A RILEVATA"));
    Serial.println(F("========================================"));

    Serial.print(F("ATQA: "));
    printHex(info.atqa[0]);
    Serial.print(' ');
    printHex(info.atqa[1]);
    Serial.println();

    Serial.print(F("UID ("));
    Serial.print(info.uid.size);
    Serial.println(F(" byte):"));

    for (uint8_t i = 0; i < info.uid.size; i++) {
        printHex(info.uid.bytes[i]);
        Serial.print(' ');
    }
    Serial.println();

    Serial.print(F("SAK: 0x"));
    printHex(info.uid.sak);
    Serial.println();

    printProbableFamily(info.uid.sak);

    Serial.println(F("========================================"));
    Serial.println();
}

// --------------------------------------------------
// SETUP
// --------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("----------------------------------------"));
    Serial.println(F("       TEST RC522 - ISO14443A"));
    Serial.println(F("----------------------------------------"));

    if (!iso.begin()) {
        Serial.println(F("ERRORE: inizializzazione RC522 fallita."));
        Serial.println(F("Controllare collegamenti, alimentazione e pin."));
        while (true) {
            delay(1000);
        }
    }

    Serial.println(F("RC522 inizializzato correttamente."));
    Serial.println(F("Avvicinare una sola carta al lettore."));
    Serial.println();
}

// --------------------------------------------------
// LOOP
// --------------------------------------------------
void loop() {
    const ISO14443A_Status status = iso.readCardSerial();

    if (status == ISO14443A_OK) {
        messaggioAttesaStampato = false;

        printCardInformation();

        // HLTA mette la carta nello stato HALT.
        // In questo modo non viene ristampata continuamente
        // finché resta appoggiata al lettore.
        const ISO14443A_Status haltStatus = iso.halt();

        if (haltStatus != ISO14443A_OK) {
            Serial.print(F("Avviso durante HLTA - "));
            printStatus(haltStatus);
        }

        Serial.println(F("Rimuovere la carta e presentarne un'altra."));
        Serial.println();
        delay(300);
    }
    else if (status == ISO14443A_TIMEOUT ||
             status == ISO14443A_NO_CARD) {

        // Il timeout è normale quando non c'è una carta
        // oppure quando la carta precedente è ancora in HALT.
        if (!messaggioAttesaStampato) {
            Serial.println(F("In attesa di una carta..."));
            messaggioAttesaStampato = true;
        }

        delay(100);
    }
    else {
        printStatus(status);
        delay(300);
    }
}
