#include "RC522_Driver.h"
#include "RC522_ISO14443A.h"
#include "RC522_NTAG21X.h"

// ============================================================
// CONFIGURAZIONE
// ============================================================
#define SS_PIN  5   // Pin SDA / SS del MFRC522
#define RST_PIN 26  // Pin RST del MFRC522

// Password e PACK con cui autenticarsi se la carta è protetta in scrittura.
// Non sono leggibili dalla carta: vanno conosciuti. Il PACK serve a controllare
// che la carta risponda come previsto dopo l'autenticazione.
const uint8_t AUTH_PASSWORD[4] = { 0x07, 0x29, 0x92, 0x70 };
const uint8_t AUTH_PACK[2]     = { 0x01, 0x37 };

// Valori di fabbrica (datasheet NXP): PWD = FFFFFFFF, PACK = 0000
const uint8_t FACTORY_PASSWORD[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
const uint8_t FACTORY_PACK[2]     = { 0x00, 0x00 };

// true: dopo aver mostrato il piano chiede conferma sul monitor seriale (invia 'S').
const bool REQUIRE_CONFIRMATION = true;

// Se la carta ha AUTHLIM attivo, ogni password errata consuma un tentativo e al
// limite la memoria protetta si blocca per sempre. Per questo, in quel caso, non
// si tenta l'autenticazione a meno di mettere true (solo se sei SICURO della password).
const bool ALLOW_AUTH_WHEN_LIMITED = false;

// ============================================================
// ISTANZE DELLE LIBRERIE
// ============================================================
RC522_Driver mfrc522_driver(SS_PIN, RST_PIN);
RC522_ISO14443A iso14443a(mfrc522_driver);
RC522_NTAG21x ntag21x(iso14443a);

// UID dell'ultima carta per cui l'autenticazione è fallita: non si ritenta finché
// non si riavvia lo sketch (cioè dopo aver corretto la password), per non consumare
// tentativi se la carta ha AUTHLIM attivo.
uint8_t failedUid[10];
uint8_t failedUidSize = 0;

struct ResetPlan {
  uint16_t pagesToWrite;     // pagine utente da riscrivere
  uint16_t pagesLocked;      // pagine utente diverse dal fabbrica ma bloccate (non azzerabili)
  bool ccDiffers;
  bool ccRestorable;         // false se contiene bit OTP già a 1 che il fabbrica non ha
  bool configDiffers;
  bool cfgLocked;
  bool staticLocks;
  bool dynamicLocks;
};

struct ResetResult {
  bool userOk;
  bool ccOk;
  bool cfgOk;
  bool pwdDone;
};

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println(F("==================================================="));
  Serial.println(F("       NTAG21x Factory Reset Tool v1.0"));
  Serial.println(F("==================================================="));
  Serial.println(F("ATTENZIONE: cancella la memoria utente e ripristina la"));
  Serial.println(F("configurazione di fabbrica. Lock e bit OTP NON si annullano."));

  SPI.begin();

  if (!iso14443a.begin()) {
    Serial.println(F("[ERRORE] Impossibile inizializzare il driver MFRC522."));
    while (true);
  }

  Serial.println(F("[OK] Driver MFRC522 inizializzato."));
  Serial.println(F("Avvicina la carta da resettare...\n"));
}

// ============================================================
// LOOP PRINCIPALE
// ============================================================
void loop() {
  if (iso14443a.readCardSerial() != ISO14443A_OK) {
    delay(100);
    return;
  }

  ISO14443A_UID uid = iso14443a.getUID();
  Serial.println(F("---------------------------------------------------"));
  Serial.println(F("[EVENTO] Carta rilevata!"));
  printUID(uid);

  if (isFailedCard(uid)) {
    Serial.println(F("[SALTATA] L'autenticazione con questa carta e' gia' fallita."));
    Serial.println(F("          Per non consumare tentativi non la ripeto: correggi la"));
    Serial.println(F("          password nello sketch, ricarica e riprova."));
    waitForCardRemoval(uid);
    return;
  }

  NTAG21x_Status status = ntag21x.begin();
  if (status != NTAG21X_OK) {
    Serial.print(F("[ERRORE] La carta non è un NTAG21x valido: "));
    Serial.println(ntag21xStatusText(status));
    waitForCardRemoval(uid);
    Serial.println(F("\n[PRONTO] In attesa della prossima carta...\n"));
    return;
  }
  printCardInfo(ntag21x.getInfo());

  resetCard(uid);

  iso14443a.halt();
  waitForCardRemoval(uid);
  Serial.println(F("\n[PRONTO] In attesa della prossima carta...\n"));
  delay(100);
}

// ============================================================
// PROCEDURA DI RESET
// ============================================================
void resetCard(const ISO14443A_UID& uid) {
  const NTAG21x_Info& info = ntag21x.getInfo();
  NTAG21x_Config cfg;

  // ---------- 1. Prima lettura: la carta è protetta in scrittura? ----------
  Serial.println(F("[1/5] Lettura iniziale della configurazione..."));
  NTAG21x_Status status = ntag21x.readConfig(cfg);

  bool needAuth = false;
  bool authlimUnknown = false;
  if (status == NTAG21X_OK) {
    needAuth = cfg.auth0 <= info.lastPage;
    Serial.print(F("      AUTH0 = "));
    Serial.print(cfg.auth0);
    Serial.print(F(", PROT = "));
    Serial.print(cfg.readProtected ? 1 : 0);
    Serial.print(F(", AUTHLIM = "));
    Serial.println(cfg.authLimit);
  } else if (status == NTAG21X_NAK) {
    // CFG0/CFG1 illeggibili: lettura protetta (PROT=1)
    needAuth = true;
    authlimUnknown = true;
    Serial.println(F("      Configurazione protetta in lettura (PROT=1)."));
  } else {
    Serial.print(F("[ERRORE] Lettura configurazione: "));
    Serial.println(ntag21xStatusText(status));
    return;
  }

  // ---------- 2. Autenticazione, se serve ----------
  if (needAuth) {
    Serial.println(F("[2/5] Carta protetta da password: autenticazione..."));

    if (!authlimUnknown && cfg.authLimit != 0 && !ALLOW_AUTH_WHEN_LIMITED) {
      Serial.print(F("[STOP] AUTHLIM = "));
      Serial.print(cfg.authLimit);
      Serial.println(F(": una password errata consuma tentativi e puo' bloccare la carta."));
      Serial.println(F("       Se sei sicuro della password imposta ALLOW_AUTH_WHEN_LIMITED = true."));
      return;
    }
    if (authlimUnknown)
      Serial.println(F("      (AUTHLIM non leggibile: faccio un solo tentativo)"));

    uint8_t pack[2] = {0, 0};
    status = ntag21x.authenticate(AUTH_PASSWORD, pack);
    if (status != NTAG21X_OK) {
      rememberFailedCard(uid);
      Serial.print(F("[ERRORE] Autenticazione: "));
      Serial.println(ntag21xStatusText(status));
      if (status == NTAG21X_AUTH_LOCKED)
        Serial.println(F("         Limite tentativi esaurito: la memoria protetta e' bloccata."));
      return;
    }
    if (pack[0] != AUTH_PACK[0] || pack[1] != AUTH_PACK[1]) {
      Serial.println(F("[STOP] Autenticazione riuscita ma il PACK non coincide con quello atteso."));
      Serial.println(F("       Non procedo: la carta potrebbe non essere quella prevista."));
      return;
    }
    Serial.println(F("      Autenticazione riuscita, PACK verificato."));
  } else {
    Serial.println(F("[2/5] Nessuna protezione in scrittura: autenticazione non necessaria."));
  }

  // ---------- 3. Lettura completa e piano ----------
  Serial.println(F("[3/5] Lettura completa e piano di reset..."));
  status = ntag21x.readConfig(cfg);
  if (status != NTAG21X_OK) {
    Serial.print(F("[ERRORE] Lettura configurazione: "));
    Serial.println(ntag21xStatusText(status));
    return;
  }
  NTAG21x_Dump dump;
  status = ntag21x.readDump(dump);
  if (status != NTAG21X_OK) {
    Serial.print(F("[ERRORE] Lettura memoria: "));
    Serial.println(ntag21xStatusText(status));
    return;
  }

  ResetPlan plan = makePlan(cfg, dump);
  printPlan(plan, cfg);

  if (!askConfirmation()) {
    Serial.println(F("[ANNULLATO] Nessuna modifica effettuata."));
    return;
  }

  // ---------- 4. Esecuzione ----------
  Serial.println(F("[4/5] Esecuzione..."));
  ResetResult res = executeReset(plan, cfg);

  // ---------- 5. Verifica ----------
  Serial.println(F("[5/5] Verifica finale..."));
  verifyAndReport(plan, res);
}

// ------------------------------------------------------------
// Contenuto di fabbrica (datasheet NXP, tab. 5-7)
// ------------------------------------------------------------
void factoryUserPage(NTAG21x_Model model, uint8_t page, uint8_t out[4]) {
  memset(out, 0, 4);
  if (model == NTAG213) {
    if (page == 4) { out[0] = 0x01; out[1] = 0x03; out[2] = 0xA0; out[3] = 0x0C; }
    if (page == 5) { out[0] = 0x34; out[1] = 0x03; out[2] = 0x00; out[3] = 0xFE; }
  } else {
    if (page == 4) { out[0] = 0x03; out[1] = 0x00; out[2] = 0xFE; out[3] = 0x00; }
  }
}

void factoryCC(NTAG21x_Model model, uint8_t cc[4]) {
  cc[0] = 0xE1;
  cc[1] = 0x10;
  cc[2] = (model == NTAG213) ? 0x12 : (model == NTAG215 ? 0x3E : 0x6D);
  cc[3] = 0x00;
}

bool configIsFactory(const NTAG21x_Config& cfg) {
  return cfg.mirrorMode == NTAG21X_MIRROR_OFF && cfg.mirrorPage == 0 && cfg.mirrorByte == 0 &&
         cfg.strongModulation && cfg.auth0 == NTAG21x_Cfg::AUTH0_DISABLED &&
         !cfg.readProtected && !cfg.nfcCounterEnabled && !cfg.nfcCounterPwdProtected &&
         cfg.authLimit == 0;
}

// ------------------------------------------------------------
// Piano
// ------------------------------------------------------------
ResetPlan makePlan(const NTAG21x_Config& cfg, const NTAG21x_Dump& dump) {
  const NTAG21x_Info& info = ntag21x.getInfo();
  ResetPlan plan;
  memset(&plan, 0, sizeof(plan));

  for (uint8_t page = info.firstUserPage; page <= info.lastUserPage; page++) {
    uint8_t factory[4];
    factoryUserPage(info.model, page, factory);
    const bool differs = !dump.pageValid[page] || memcmp(dump.memory[page], factory, 4) != 0;
    if (!differs) continue;
    if (ntag21x.isPageLocked(page, cfg)) plan.pagesLocked++;
    else plan.pagesToWrite++;
  }

  uint8_t factory[4];
  factoryCC(info.model, factory);
  plan.ccDiffers = memcmp(cfg.capabilityContainer, factory, 4) != 0;
  plan.ccRestorable = !ntag21x.isPageLocked(3, cfg);
  for (uint8_t i = 0; i < 4; i++)
    if (cfg.capabilityContainer[i] & ~factory[i]) plan.ccRestorable = false;

  plan.configDiffers = !configIsFactory(cfg);
  plan.cfgLocked = cfg.configLocked;
  plan.staticLocks = cfg.staticLock[0] != 0 || cfg.staticLock[1] != 0;
  plan.dynamicLocks = cfg.dynamicLock[0] != 0 || cfg.dynamicLock[1] != 0 || cfg.dynamicLock[2] != 0;
  return plan;
}

void printPlan(const ResetPlan& plan, const NTAG21x_Config& cfg) {
  Serial.println(F("\n--- PIANO DI RESET ---"));
  Serial.print(F("Memoria utente:  "));
  Serial.print(plan.pagesToWrite);
  Serial.println(F(" pagine da riscrivere (NDEF vuoto + zeri)"));
  if (plan.pagesLocked) {
    Serial.print(F("                 "));
    Serial.print(plan.pagesLocked);
    Serial.println(F(" pagine diverse dal fabbrica ma BLOCCATE: non azzerabili"));
  }

  Serial.print(F("Cap. container:  "));
  if (!plan.ccDiffers) Serial.println(F("gia' di fabbrica"));
  else if (plan.ccRestorable) Serial.println(F("da ripristinare"));
  else Serial.println(F("diverso dal fabbrica, NON ripristinabile (bit OTP gia' scritti)"));

  Serial.print(F("Configurazione:  "));
  if (plan.cfgLocked && plan.configDiffers) Serial.println(F("NON modificabile (CFGLCK attivo)"));
  else if (plan.configDiffers) Serial.println(F("da ripristinare (AUTH0, PROT, AUTHLIM, mirror, contatore, modulazione)"));
  else Serial.println(F("gia' di fabbrica"));

  if (plan.cfgLocked && cfg.auth0 <= ntag21x.getInfo().lastPage) {
    Serial.println(F("Password:        NON verranno toccati PWD/PACK: la protezione resta attiva"));
    Serial.println(F("                 e non si puo' togliere (CFGLCK)"));
  } else {
    Serial.println(F("Password:        PWD/PACK verranno riscritti a FFFFFFFF / 0000"));
  }

  if (plan.staticLocks || plan.dynamicLocks || plan.cfgLocked) {
    Serial.println(F("Irreversibili gia' presenti (restano dopo il reset):"));
    if (plan.staticLocks) Serial.println(F("  - lock statici"));
    if (plan.dynamicLocks) Serial.println(F("  - lock dinamici"));
    if (plan.cfgLocked) Serial.println(F("  - CFGLCK"));
  }
  Serial.println();
}

bool askConfirmation() {
  if (!REQUIRE_CONFIRMATION) return true;

  while (Serial.available()) Serial.read();
  Serial.println(F("[CONFERMA] Invia 'S' entro 20 secondi per procedere (altro = annulla)."));
  Serial.println(F("           Tieni la carta sul lettore."));

  const unsigned long start = millis();
  while (millis() - start < 20000UL) {
    if (Serial.available()) {
      const char c = Serial.read();
      if (c == '\n' || c == '\r') continue;
      return c == 'S' || c == 's';
    }
    delay(10);
  }
  Serial.println(F("[CONFERMA] Tempo scaduto."));
  return false;
}

// ------------------------------------------------------------
// Esecuzione
// Ordine: memoria utente -> CC -> configurazione (AUTH0 per ultimo) -> PWD/PACK.
// Se la configurazione non si riesce a ripristinare, PWD/PACK restano come sono:
// non si lascia una carta ancora protetta con la password di fabbrica.
// ------------------------------------------------------------
bool stepOk(const __FlashStringHelper* name, NTAG21x_Status status) {
  if (status == NTAG21X_OK) return true;
  Serial.print(F("  [ERRORE] "));
  Serial.print(name);
  Serial.print(F(": "));
  Serial.println(ntag21xStatusText(status));
  return false;
}

ResetResult executeReset(const ResetPlan& plan, const NTAG21x_Config& cfg) {
  const NTAG21x_Info& info = ntag21x.getInfo();
  ResetResult res;
  res.userOk = true;
  res.ccOk = true;
  res.cfgOk = true;
  res.pwdDone = false;

  // ----- Memoria utente (a blocchi di pagine consecutive non bloccate) -----
  Serial.print(F("  Memoria utente "));
  int page = info.firstUserPage;
  while (page <= info.lastUserPage) {
    if (ntag21x.isPageLocked(page, cfg)) { page++; continue; }

    uint8_t buffer[64];
    int count = 0;
    while (page + count <= info.lastUserPage && count < 16 &&
           !ntag21x.isPageLocked(page + count, cfg)) {
      factoryUserPage(info.model, page + count, buffer + count * 4);
      count++;
    }
    NTAG21x_Status status = ntag21x.writeBytes(
        (uint16_t)(page - info.firstUserPage) * 4, buffer, (uint16_t)count * 4);
    if (status == NTAG21X_OK) {
      Serial.print('.');
    } else {
      Serial.print('X');
      res.userOk = false;
      Serial.println();
      Serial.print(F("  [ERRORE] pagine "));
      Serial.print(page);
      Serial.print('-');
      Serial.print(page + count - 1);
      Serial.print(F(": "));
      Serial.println(ntag21xStatusText(status));
    }
    page += count;
  }
  Serial.println();

  // ----- Capability container (solo se ripristinabile) -----
  if (plan.ccDiffers && plan.ccRestorable) {
    uint8_t factory[4], check[4];
    factoryCC(info.model, factory);
    // Il chip mette in OR i dati con il CC: il risultato e' il valore di fabbrica
    // perche' il CC attuale non ha bit in piu'.
    res.ccOk = stepOk(F("CC (scrittura)"), ntag21x.writePage(3, factory)) &&
               stepOk(F("CC (verifica)"), ntag21x.readPage(3, check));
    if (res.ccOk && memcmp(check, factory, 4) != 0) {
      Serial.println(F("  [ERRORE] CC: valore diverso dopo la scrittura"));
      res.ccOk = false;
    }
    if (res.ccOk) Serial.println(F("  Capability container ripristinato"));
  } else if (plan.ccDiffers) {
    res.ccOk = false;
  }

  // ----- Configurazione: AUTH0 per ultimo, cosi' un errore non lascia una
  //       configurazione a meta' con la protezione gia' tolta -----
  if (plan.cfgLocked) {
    res.cfgOk = !plan.configDiffers;
  } else if (plan.configDiffers) {
    res.cfgOk = stepOk(F("AUTHLIM"), ntag21x.setAuthLimit(0)) &&
                stepOk(F("PROT"), ntag21x.setReadProtection(false)) &&
                stepOk(F("NFC_CNT_PWD_PROT"), ntag21x.setNfcCounterPasswordProtection(false)) &&
                stepOk(F("NFC_CNT_EN"), ntag21x.setNfcCounter(false)) &&
                stepOk(F("MIRROR"), ntag21x.setMirror(NTAG21X_MIRROR_OFF)) &&
                stepOk(F("STRG_MOD_EN"), ntag21x.setStrongModulation(true)) &&
                stepOk(F("AUTH0"), ntag21x.setAuth0(NTAG21x_Cfg::AUTH0_DISABLED));
    if (res.cfgOk) Serial.println(F("  Configurazione ripristinata"));
  }

  // ----- Password: solo se la protezione e' (o resta) disattivata -----
  const bool protectionOff = res.cfgOk || cfg.auth0 > info.lastPage;
  if (protectionOff) {
    res.pwdDone = stepOk(F("PWD"), ntag21x.setPassword(FACTORY_PASSWORD)) &&
                  stepOk(F("PACK"), ntag21x.setPack(FACTORY_PACK));
    if (res.pwdDone) {
      uint8_t pack[2] = {0xFF, 0xFF};
      NTAG21x_Status status = ntag21x.authenticate(FACTORY_PASSWORD, pack);
      if (status == NTAG21X_OK && pack[0] == FACTORY_PACK[0] && pack[1] == FACTORY_PACK[1])
        Serial.println(F("  Password di fabbrica scritta e verificata con PWD_AUTH"));
      else
        Serial.println(F("  Password di fabbrica scritta (verifica con PWD_AUTH non riuscita)"));
    }
  } else {
    Serial.println(F("  PWD/PACK non toccati: la protezione e' ancora attiva"));
  }
  return res;
}

// ------------------------------------------------------------
// Verifica e rapporto
// ------------------------------------------------------------
void verifyAndReport(const ResetPlan& plan, const ResetResult& res) {
  const NTAG21x_Info& info = ntag21x.getInfo();
  NTAG21x_Config cfg;
  NTAG21x_Status status = ntag21x.readConfig(cfg);
  if (status != NTAG21X_OK) {
    Serial.print(F("[ERRORE] Rilettura configurazione: "));
    Serial.println(ntag21xStatusText(status));
    return;
  }
  NTAG21x_Dump dump;
  status = ntag21x.readDump(dump);
  if (status != NTAG21X_OK) {
    Serial.print(F("[ERRORE] Rilettura memoria: "));
    Serial.println(ntag21xStatusText(status));
    return;
  }

  uint16_t wrong = 0, lockedWrong = 0;
  for (uint8_t page = info.firstUserPage; page <= info.lastUserPage; page++) {
    uint8_t factory[4];
    factoryUserPage(info.model, page, factory);
    if (memcmp(dump.memory[page], factory, 4) == 0) continue;
    if (ntag21x.isPageLocked(page, cfg)) lockedWrong++;
    else wrong++;
  }

  uint8_t factoryCc[4];
  factoryCC(info.model, factoryCc);
  const bool ccFactory = memcmp(cfg.capabilityContainer, factoryCc, 4) == 0;
  const bool cfgFactory = configIsFactory(cfg);

  Serial.println(F("\n--- ESITO ---"));
  Serial.print(F("Memoria utente:   "));
  if (wrong > 0) Serial.println(F("NON ripristinata"));
  else if (lockedWrong > 0) Serial.println(F("azzerata, tranne le pagine bloccate"));
  else Serial.println(F("di fabbrica"));
  if (wrong) { Serial.print(F("                  pagine ancora errate: ")); Serial.println(wrong); }
  if (lockedWrong) {
    Serial.print(F("                  pagine bloccate con dati non di fabbrica: "));
    Serial.println(lockedWrong);
  }
  Serial.print(F("Cap. container:   "));
  Serial.println(ccFactory ? F("di fabbrica") : F("diverso dal fabbrica"));
  Serial.print(F("Configurazione:   "));
  Serial.println(cfgFactory ? F("di fabbrica") : F("NON di fabbrica"));
  Serial.print(F("Password:         "));
  Serial.println(res.pwdDone ? F("riportata a FFFFFFFF / 0000") : F("non modificata"));

  const bool anyLock = cfg.staticLock[0] || cfg.staticLock[1] ||
                       cfg.dynamicLock[0] || cfg.dynamicLock[1] || cfg.dynamicLock[2];
  if (anyLock || cfg.configLocked) {
    Serial.println(F("Irreversibili rimasti:"));
    if (cfg.staticLock[0] || cfg.staticLock[1]) Serial.println(F("  - lock statici"));
    if (cfg.dynamicLock[0] || cfg.dynamicLock[1] || cfg.dynamicLock[2])
      Serial.println(F("  - lock dinamici"));
    if (cfg.configLocked) Serial.println(F("  - CFGLCK"));
  }

  Serial.println(F("---------------------------------------------------"));
  const bool complete = wrong == 0 && lockedWrong == 0 && ccFactory && cfgFactory &&
                        res.pwdDone && !anyLock && !cfg.configLocked;
  if (complete) {
    Serial.println(F("[RESET COMPLETATO] La carta e' in configurazione di fabbrica."));
  } else {
    Serial.println(F("[RESET PARZIALE] Vedi i punti sopra: alcuni elementi non sono"));
    Serial.println(F("                 ripristinabili (lock/OTP) o non e' stato possibile."));
  }
}

// ============================================================
// FUNZIONI DI SUPPORTO
// ============================================================
bool isFailedCard(const ISO14443A_UID& uid) {
  return failedUidSize != 0 && uid.size == failedUidSize &&
         memcmp(uid.bytes, failedUid, uid.size) == 0;
}

void rememberFailedCard(const ISO14443A_UID& uid) {
  failedUidSize = uid.size;
  memcpy(failedUid, uid.bytes, uid.size);
}

void printUID(const ISO14443A_UID& uid) {
  Serial.print(F("[UID] "));
  for (uint8_t i = 0; i < uid.size; i++) {
    if (uid.bytes[i] < 0x10) Serial.print(F("0"));
    Serial.print(uid.bytes[i], HEX);
    if (i < uid.size - 1) Serial.print(F(":"));
  }
  Serial.println();
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
  Serial.print(info.userBytes);
  Serial.print(F(" byte utente (pagine "));
  Serial.print(info.firstUserPage);
  Serial.print(F("-"));
  Serial.print(info.lastUserPage);
  Serial.println(F(")"));
}

void waitForCardRemoval(const ISO14443A_UID& originalUID) {
  Serial.print(F("[ATTESA] Rimuovi la carta dal lettore..."));

  uint8_t consecutiveMisses = 0;
  constexpr uint8_t REQUIRED_MISSES = 3;

  while (consecutiveMisses < REQUIRED_MISSES) {
    // WUPA: risveglia le carte in HALT
    ISO14443A_Status status = iso14443a.wakeup();

    if (status == ISO14443A_TIMEOUT || status == ISO14443A_NO_CARD) {
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

    // Una carta (la stessa o un'altra) e' ancora presente
    consecutiveMisses = 0;
    iso14443a.halt();
    delay(300);
  }

  Serial.println(F(" fatto."));
  delay(500);
}