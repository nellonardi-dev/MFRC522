# Configurazione multimodulo MFRC522 con ESP32

## 1. Introduzione

Questa guida descrive come collegare e gestire più moduli **MFRC522**
utilizzando un unico bus SPI di un **ESP32 classico (ESP32-WROOM /
DevKit V1)**.

La configurazione proposta utilizza:

-   un bus SPI condiviso da tutti i lettori;
-   un pin **SS (Slave Select)** dedicato a ciascun modulo;
-   un unico pin **RST (Reset)** condiviso da tutti i moduli;
-   una funzione di reset hardware globale;
-   una funzione di inizializzazione di tutti i lettori;
-   una funzione `restart()` che combina le due operazioni.

L'obiettivo è mantenere separata la gestione dell'hardware dalla
gestione dei singoli driver `RC522_Driver`.

> **Nota:** i pin riportati sono un esempio per ESP32 classico.
> Verificare sempre il pinout della propria scheda: ESP32-S2, S3, C3 e
> altre varianti possono avere assegnazioni e limitazioni differenti.

## 2. Collegamenti hardware

Tutti i moduli MFRC522 condividono le linee SPI. Ogni modulo deve invece
avere il proprio pin SS.

### Assegnazione pin di esempio

  Segnale               ESP32 Moduli MFRC522
  ----------------- --------- ---------------------------------------
  SCK                 GPIO 18 SCK di tutti i moduli
  MOSI                GPIO 23 MOSI di tutti i moduli
  MISO                GPIO 19 MISO di tutti i moduli
  SS lettore 1        GPIO 25 SDA/SS del modulo 1
  SS lettore 2        GPIO 27 SDA/SS del modulo 2
  SS lettore 3        GPIO 32 SDA/SS del modulo 3
  RESET condiviso     GPIO 26 RST di tutti i moduli
  Alimentazione         3,3 V VCC, secondo le specifiche del modulo
  Massa                   GND GND di tutti i moduli

I pin SPI 18, 19 e 23 sono quelli comunemente utilizzati come bus VSPI
sull'ESP32 classico. Se il progetto usa una diversa configurazione SPI,
adeguare i collegamenti e l'inizializzazione del bus.

### Regole di collegamento

-   Collegare in parallelo SCK, MOSI e MISO a tutti i lettori.
-   Collegare ogni ingresso SS a un GPIO distinto.
-   Collegare insieme i pin RST di tutti i moduli e portarli al GPIO di
    reset condiviso.
-   Collegare tutte le masse GND.
-   Utilizzare livelli logici compatibili con ESP32: i GPIO dell'ESP32
    lavorano a 3,3 V.
-   Verificare l'alimentazione richiesta dal breakout MFRC522
    utilizzato. Non dare per scontato che ogni scheda accetti 5 V sui
    pin di alimentazione o sui segnali.

Quando un lettore è selezionato, il suo SS viene portato LOW; gli SS
degli altri lettori devono rimanere HIGH. La libreria e la
configurazione SPI devono garantire che la selezione dei dispositivi sia
gestita correttamente.

## 3. Configurazione nello sketch

La configurazione può essere mantenuta in un unico punto dello sketch:

``` cpp
#include "RC522_Driver.h"

// Pin RESET condiviso da tutti i moduli
constexpr uint8_t RST_SHARED = 26;

// Pin SS individuali
constexpr uint8_t SS_PINS[] = {
    25,  // Lettore 1
    27,  // Lettore 2
    32   // Lettore 3
};

// Numero totale di lettori configurati
constexpr size_t NUM_READERS =
    sizeof(SS_PINS) / sizeof(SS_PINS[0]);

// Un'istanza del driver per ogni lettore
RC522_Driver* readers[NUM_READERS] = {};
```

In questo esempio ogni oggetto viene costruito utilizzando il proprio
pin SS e senza assegnargli un pin RESET dedicato:

``` cpp
readers[i] = new RC522_Driver(SS_PINS[i]);
```

Il reset hardware è infatti gestito dallo sketch tramite `RST_SHARED`.

## 4. Perché separare hardReset() e start()

Nella configurazione con RESET condiviso è importante distinguere il
reset fisico dei chip dalla loro inizializzazione tramite software.

### 4.1 `hardReset()`: reset hardware globale

La funzione `hardReset()` agisce direttamente sui pin dell'ESP32:

1.  imposta tutti i pin SS come uscite;
2.  porta tutti gli SS a HIGH, in modo da deselezionare i lettori;
3.  configura il pin RESET condiviso come uscita;
4.  porta RST a LOW per generare il reset hardware;
5.  riporta RST a HIGH;
6.  attende il tempo necessario prima di iniziare le comunicazioni.

Il reset è **globale**: poiché tutti i moduli sono collegati allo stesso
RST, vengono resettati contemporaneamente.

Esempio:

``` cpp
void hardReset() {

    // Deseleziona tutti i lettori
    for (size_t i = 0; i < NUM_READERS; i++) {
        pinMode(SS_PINS[i], OUTPUT);
        digitalWrite(SS_PINS[i], HIGH);
    }

    // Reset hardware condiviso
    pinMode(RST_SHARED, OUTPUT);
    digitalWrite(RST_SHARED, LOW);
    delay(10);

    digitalWrite(RST_SHARED, HIGH);
    delay(50);
}
```

I tempi dell'esempio sono intervalli conservativi da verificare rispetto
al datasheet del chip e alle caratteristiche del modulo utilizzato.

**Perché è importante:** un reset hardware iniziale porta tutti i chip
in una condizione di reset comune prima che il programma inizi a
configurarli. È inoltre utile per recuperare l'intero gruppo di lettori
quando si desidera ripartire da uno stato hardware noto.

### 4.2 `start()`: inizializzazione software di tutti i lettori

Dopo il reset hardware, ogni modulo deve essere inizializzato tramite la
propria istanza del driver.

La funzione `start()` percorre l'array dei lettori e chiama `begin()` su
ciascun oggetto:

``` cpp
void start() {

    for (size_t i = 0; i < NUM_READERS; i++) {

        if (readers[i] == nullptr) {
            continue;
        }

        readers[i]->begin();
    }
}
```

La chiamata:

``` cpp
readers[i]->begin();
```

è il punto in cui viene inizializzato il singolo modulo.

Il ciclo esegue una chiamata a `begin()` per ogni lettore valido.
`begin()` non sostituisce il reset hardware condiviso: esegue la
procedura di inizializzazione prevista dal driver.

> In questa libreria `begin()` è considerata una funzione senza valore
> di ritorno (`void`). Di conseguenza, il fatto che sia stata chiamata
> non dimostra da solo che il chip abbia risposto correttamente. Per
> verificare il funzionamento servono controlli successivi, ad esempio
> la lettura del registro versione o il self-test, se supportato dal
> driver.

### 4.3 `restart()`: reset e inizializzazione coordinati

Una volta disponibili `hardReset()` e `start()`, è possibile definire
una funzione `restart()`:

``` cpp
void restart() {

    hardReset();
    start();
}
```

Questa funzione rappresenta la procedura completa per riavviare tutti i
lettori:

1.  esegue il reset hardware condiviso;
2.  inizializza nuovamente ogni modulo, uno alla volta.

È consigliabile utilizzare `restart()` all'avvio del sistema e ogni
volta che si desidera ripristinare l'intero gruppo di lettori.

## 5. Creazione delle istanze

Gli oggetti driver devono essere creati una sola volta, prima di
chiamare `restart()`:

``` cpp
void createReaders() {

    for (size_t i = 0; i < NUM_READERS; i++) {

        if (readers[i] == nullptr) {
            readers[i] = new RC522_Driver(SS_PINS[i]);
        }
    }
}
```

La creazione delle istanze è separata dal reset e dall'inizializzazione:

-   `createReaders()` crea gli oggetti C++;
-   `hardReset()` resetta fisicamente i chip;
-   `start()` inizializza gli oggetti driver;
-   `restart()` esegue reset e inizializzazione in sequenza.

La funzione `createReaders()` non deve essere richiamata a ogni restart:
le istanze già create vanno riutilizzate. Su sistemi embedded è
opportuno valutare anche il consumo di RAM e la strategia di
allocazione; se il progetto lo richiede, si può preferire una gestione
statica degli oggetti.

## 6. Sequenza di avvio consigliata

Una sequenza di avvio essenziale può essere:

``` cpp
void setup() {

    Serial.begin(115200);
    delay(500);

    createReaders();

    // Reset hardware globale + inizializzazione di tutti i lettori
    restart();

    // Da qui è possibile eseguire i test diagnostici
    // e avviare la logica applicativa.
}

void loop() {
}
```

La sequenza è quindi:

``` text
Avvio ESP32
    |
    v
createReaders()
    |
    v
restart()
    |
    +--> hardReset()
    |       |
    |       +--> tutti gli SS HIGH
    |       +--> RST LOW
    |       +--> RST HIGH
    |
    +--> start()
            |
            +--> lettore 1: begin()
            +--> lettore 2: begin()
            +--> lettore 3: begin()
    |
    v
Test diagnostici / applicazione
```

## 7. Quando richiamare restart()

`restart()` è utile:

-   durante la fase di avvio, per inizializzare tutti i moduli in modo
    coordinato;
-   dopo una condizione di errore che richieda il reset hardware
    dell'intero gruppo;
-   dopo un'interruzione o una riconfigurazione hardware, quando si
    vuole riportare tutti i chip a una condizione iniziale comune.

Poiché il pin RESET è condiviso, non è possibile resettare un singolo
lettore senza resettare anche gli altri. Dopo un reset condiviso è
quindi necessario eseguire nuovamente `begin()` su **tutti** i lettori.

Se invece si richiama soltanto `start()`, senza un reset hardware
precedente, tutti i driver vengono inizializzati nuovamente ma i chip
non ricevono un nuovo impulso sul pin RST.

## 8. Diagnostica dei singoli lettori

Dopo `restart()` è possibile verificare ogni modulo individualmente.

Esempi di controlli utili:

-   lettura del registro versione (`getVersion()`);
-   self-test interno (`selfTest()`), se implementato e supportato;
-   verifica delle impostazioni RF;
-   controllo del guadagno antenna;
-   test del CRC hardware e confronto con un calcolo software.

È importante distinguere tra **inizializzazione invocata** e **lettore
verificato**. Se `begin()` non restituisce uno stato, il risultato della
diagnostica deve basarsi su letture e test successivi.

## 9. Limiti e considerazioni

-   Il reset condiviso riavvia tutti i moduli contemporaneamente.
-   Ogni lettore deve avere un SS individuale.
-   Tutti i moduli condividono le linee SPI.
-   Gli SS non selezionati devono restare HIGH.
-   La stabilità del bus dipende anche da cablaggio, alimentazione,
    lunghezza dei fili e gestione corretta dei segnali.
-   Il numero di lettori utilizzabili dipende dai GPIO disponibili,
    dalla memoria e dalle caratteristiche dell'applicazione.
-   La mappatura dei pin e la configurazione del bus SPI devono essere
    adattate alla specifica scheda ESP32.

## 10. Riepilogo dell'architettura

  -----------------------------------------------------------------------
  Funzione                            Responsabilità
  ----------------------------------- -----------------------------------
  `createReaders()`                   Crea una sola volta le istanze
                                      `RC522_Driver`

  `hardReset()`                       Esegue il reset hardware condiviso
                                      di tutti i chip

  `start()`                           Chiama `begin()` su ogni istanza
                                      valida

  `restart()`                         Esegue `hardReset()` seguito da
                                      `start()`

  Diagnostica                         Verifica separatamente il
                                      funzionamento di ogni lettore
  -----------------------------------------------------------------------

Questa architettura mantiene semplice la libreria: il driver si occupa
del singolo MFRC522, mentre lo sketch conosce la topologia hardware e
coordina il gruppo di moduli.

------------------------------------------------------------------------

**Configurazione di riferimento:** ESP32 classico, bus SPI condiviso,
più MFRC522 con SS individuali e RESET hardware comune.