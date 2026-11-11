#include "RC522_Driver.h"
#include "RC522_ISO14443A.h"
#include "RC522_NTAG21X.h"

// ============================================================
// CONFIGURAZIONE PIN (Modifica in base al tuo microcontrollore)
// ============================================================
#define SS_PIN  5  // Pin SDA / SS del MFRC522
#define RST_PIN 26   // Pin RST del MFRC522 (opzionale, ma consigliato)

// ============================================================
// ISTANZIE DELLE LIBRERIE
// ============================================================
RC522_Driver mfrc522_driver(SS_PIN, RST_PIN);
RC522_ISO14443A iso14443a(mfrc522_driver);
RC522_NTAG21x ntag21x(iso14443a);

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Attende l'apertura della porta seriale (necessario per alcune board)
  }

  Serial.println(F("==================================================="));
  Serial.println(F("       NTAG21x Auto-Dump Tool v1.2"));
  Serial.println(F("==================================================="));

  SPI.begin();

  if (!iso14443a.begin()) {
    Serial.println(F("[ERRORE] Impossibile inizializzare il driver MFRC522."));
    Serial.println(F("Controlla i collegamenti SPI (SS, MOSI, MISO, SCK, RST)."));
    while (true); // Blocco in caso di errore hardware
  }

  Serial.println(F("[OK] Driver MFRC522 inizializzato con successo."));
  Serial.println(F("Avvicina un tag NTAG213, NTAG215 o NTAG216 al lettore...\n"));
}

// ============================================================
// LOOP PRINCIPALE
// ============================================================
void loop() {
  // 1. Rilevamento carta (REQA + Anticollision)
  if (iso14443a.readCardSerial() == ISO14443A_OK) {
    Serial.println(F("---------------------------------------------------"));
    Serial.println(F("[EVENTO] Carta rilevata!"));
    ISO14443A_UID originalUID = iso14443a.getUID();
    // Stampa UID
    printUID(originalUID);

    // 2. Inizializzazione specifica per NTAG21x
    NTAG21x_Status status = ntag21x.begin();
    if (status != NTAG21X_OK) {
      Serial.print(F("[ERRORE] La carta non è un NTAG21x valido: "));
      Serial.println(ntag21xStatusText(status));
      waitForCardRemoval(originalUID);
      Serial.println(F("\n[PRONTO] In attesa della prossima carta...\n"));
      return;
    }

    // 3. Stampa informazioni sulla carta
    printCardInfo(ntag21x.getInfo());

    // 4. Dump della memoria
    Serial.println(F("[INFO] Lettura memoria in corso (potrebbe richiedere alcuni secondi)..."));
    NTAG21x_Dump dump;
    status = ntag21x.readDump(dump);

    // 5. Lettura della configurazione (a carta ancora attiva: va fatta PRIMA dell'halt)
    NTAG21x_Config cfg;
    NTAG21x_Status cfgStatus = readConfigSafe(cfg, status != NTAG21X_OK);

    // Firma di originalità e contatore NFC (solo se attivo e non protetto da password)
    uint8_t signature[NTAG21x_Protocol::SIGNATURE_SIZE] = {0};
    NTAG21x_Status sigStatus = ntag21x.readSignature(signature);
    uint32_t counter = 0;
    bool counterTried = cfg.configValid && cfg.nfcCounterEnabled && !cfg.nfcCounterPwdProtected;
    NTAG21x_Status counterStatus = counterTried ? ntag21x.readCounter(counter) : NTAG21X_OK;

    // 6. Ora la carta può essere messa in HALT
    iso14443a.halt();

    // 7. Stampa del Dump formattato
    printDump(dump, ntag21x.getInfo());

    // 8. Stato di configurazione: valori grezzi + interpretazione
    printConfigReport(cfg, cfgStatus);
    printExtras(sigStatus, signature, counterTried, counterStatus, counter);

    // 9. Riepilogo operazione
    Serial.println(F("---------------------------------------------------"));
    if (status == NTAG21X_OK) {
      Serial.println(F("[SUCCESSO] Dump dell'intera memoria completato con successo."));
    } else if (status == NTAG21X_PARTIAL_DUMP) {
      Serial.println(F("[ATTENZIONE] Dump parziale. Alcune pagine sono protette da password o bloccate in scrittura (NAK)."));
    } else {
      Serial.print(F("[ERRORE] Fallimento durante il dump: "));
      Serial.println(ntag21xStatusText(status));
    }

    // 10. Attendi la rimozione della carta per evitare letture multiple
    waitForCardRemoval(originalUID);
    Serial.println(F("\n[PRONTO] In attesa della prossima carta...\n"));
  }

  delay(100); // Piccola pausa per non sovraccaricare il loop
}

// ============================================================
// FUNZIONI DI SUPPORTO (HELPER)
// ============================================================

// Legge la configurazione. Se il dump non è andato a buon fine (NAK su pagine
// protette) il tag potrebbe non essere più nello stato ACTIVE: per precauzione
// lo si riseleziona prima di leggere la configurazione.
NTAG21x_Status readConfigSafe(NTAG21x_Config& cfg, bool reactivate) {
  if (reactivate) {
    if (iso14443a.wakeup() != ISO14443A_OK ||
        iso14443a.selectCard() != ISO14443A_OK) {
      memset(&cfg, 0, sizeof(cfg));
      return NTAG21X_TIMEOUT;
    }
  }
  return ntag21x.readConfig(cfg);
}

void printConfigReport(const NTAG21x_Config& cfg, NTAG21x_Status cfgStatus) {
  Serial.println(F("\n--- STATO DI CONFIGURAZIONE ---"));

  if (!cfg.lockValid) {
    Serial.print(F("[ERRORE] Configurazione non leggibile: "));
    Serial.println(ntag21xStatusText(cfgStatus));
    Serial.println(F("--- FINE CONFIGURAZIONE ---\n"));
    return;
  }

  // Valori grezzi e decodificati
  ntag21x.printConfig(Serial, cfg);
  Serial.println();

  // Interpretazione in linguaggio naturale
  ntag21x.interpretConfig(Serial, cfg);

  if (cfgStatus == NTAG21X_NAK) {
    Serial.println(F("[NOTA] CFG0/CFG1 protetti da password: lettura parziale."));
  } else if (cfgStatus != NTAG21X_OK) {
    Serial.print(F("[NOTA] Lettura configurazione incompleta: "));
    Serial.println(ntag21xStatusText(cfgStatus));
  }
  Serial.println(F("--- FINE CONFIGURAZIONE ---\n"));
}

void printExtras(NTAG21x_Status sigStatus, const uint8_t* signature,
                 bool counterTried, NTAG21x_Status counterStatus, uint32_t counter) {
  Serial.println(F("--- INFORMAZIONI AGGIUNTIVE ---"));
  Serial.print(F("[FIRMA] "));
  if (sigStatus == NTAG21X_OK) {
    for (uint8_t i = 0; i < NTAG21x_Protocol::SIGNATURE_SIZE; i++) {
      if (signature[i] < 0x10) Serial.print(F("0"));
      Serial.print(signature[i], HEX);
    }
    Serial.println();
  } else {
    Serial.println(ntag21xStatusText(sigStatus));
  }
  Serial.print(F("[CONTATORE] "));
  if (!counterTried) {
    Serial.println(F("non letto (disattivato o protetto da password)"));
  } else if (counterStatus == NTAG21X_OK) {
    Serial.println(counter);
  } else {
    Serial.println(ntag21xStatusText(counterStatus));
  }
  Serial.println();
}

void printUID(const ISO14443A_UID& uid) {
  Serial.print(F("[UID] "));
  for (uint8_t i = 0; i < uid.size; i++) {
    if (uid.bytes[i] < 0x10) Serial.print(F("0"));
    Serial.print(uid.bytes[i], HEX);
    if (i < uid.size - 1) Serial.print(F(":"));
  }
  Serial.print(F(" ("));
  Serial.print(uid.size * 8);
  Serial.println(F(" bit)"));
}

void printCardInfo(const NTAG21x_Info& info) {
  Serial.print(F("[MODELLO] "));
  switch (info.model) {
    case NTAG213: Serial.println(F("NTAG213")); break;
    case NTAG215: Serial.println(F("NTAG215")); break;
    case NTAG216: Serial.println(F("NTAG216")); break;
    default:      Serial.println(F("Sconosciuto")); break;
  }
  Serial.print(F("[MEMORIA] "));
  Serial.print(info.totalBytes);
  Serial.print(F(" byte totali ("));
  Serial.print(info.userBytes);
  Serial.println(F(" byte utente)"));
  Serial.print(F("[PAGINE]  Totale: "));
  Serial.print(info.lastPage + 1); // +1 perché si parte da 0
  Serial.print(F(" | Utente: da "));
  Serial.print(info.firstUserPage);
  Serial.print(F(" a "));
  Serial.println(info.lastUserPage);
}

void printDump(const NTAG21x_Dump& dump, const NTAG21x_Info& info) {
  Serial.println(F("\n--- INIZIO DUMP MEMORIA ---"));
  Serial.println(F(" Pag | Adr  |Byte 0|Byte 1|Byte 2|Byte 3| Stato"));
  Serial.println(F("-----|------|------|------|------|------|------"));

  for (uint8_t page = 0; page <= dump.lastPage; page++) {

    Serial.print(F(" "));
    if(page < 100)Serial.print(F(" "));
    if(page < 10)Serial.print(F(" "));
    Serial.print(page);
    // Formattazione numero pagina in hex
    Serial.print(F(" |  "));
    if (page < 0x10) Serial.print(F("0"));
    Serial.print(page, HEX);
    Serial.print(F("  |"));

    // Stampa i 4 byte della pagina
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t val = dump.memory[page][i];
      Serial.print(F("  "));
      if (val < 0x10) Serial.print(F("0"));
      Serial.print(val, HEX);
      Serial.print(F("  |"));
    }

    // Stato della pagina
    if (dump.pageValid[page]) {
      Serial.print(F(" [OK]"));
    } else {
      Serial.print(F("[NAK/LOCKED]"));
    }

    // Annotazioni speciali per pagine note (opzionale ma utile)
    if (page == 0) Serial.print(F("  <- UID (parte) / Manufacturer"));
    else if (page == 2) Serial.print(F("  <- Static Lock Bytes"));
    else if (page == 3) Serial.print(F("  <- Capability Container (CC)"));
    else if (page == info.dynamicLockPage) Serial.print(F("  <- Dynamic Lock Bytes"));
    else if (page >= info.configStartPage && page < info.configStartPage + 4) Serial.print(F("  <- Configuration / Password"));

    Serial.println();
  }
  Serial.println(F("--- FINE DUMP MEMORIA ---\n"));
}

void waitForCardRemoval(const ISO14443A_UID& originalUID) {

    Serial.print(F("[ATTESA] Rimuovi la carta dal lettore..."));

    uint8_t consecutiveMisses = 0;
    constexpr uint8_t REQUIRED_MISSES = 3;

    while (consecutiveMisses < REQUIRED_MISSES) {

        // WUPA: risveglia le carte in HALT
        ISO14443A_Status status = iso14443a.wakeup();

        if (status == ISO14443A_TIMEOUT ||
            status == ISO14443A_NO_CARD) {

            consecutiveMisses++;
            delay(300);
            continue;
        }

        // Errori diversi da timeout: non considerarli rimozione
        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        // Una carta ha risposto: esegui anticollisione e SELECT
        status = iso14443a.selectCard();

        if (status != ISO14443A_OK) {
            delay(300);
            continue;
        }

        // Recupera UID della carta rilevata
        const ISO14443A_UID& detectedUID = iso14443a.getUID();

        bool sameCard =
            detectedUID.size == originalUID.size &&
            memcmp(detectedUID.bytes,
                   originalUID.bytes,
                   originalUID.size) == 0;

        if (sameCard) {
            // La carta originale è ancora presente
            consecutiveMisses = 0;
        } else {
            // È stata rilevata una carta con UID differente
            consecutiveMisses = 0;
        }

        // Riporta in HALT la carta selezionata
        iso14443a.halt();

        delay(300);
    }

    Serial.println(F(" fatto."));
    delay(500);
}