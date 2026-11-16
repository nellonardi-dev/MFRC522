# Multi-Module MFRC522 Configuration with ESP32

## 1. Introduction

This guide explains how to connect and manage multiple **MFRC522**
modules using a single SPI bus on a **classic ESP32 (ESP32-WROOM /
DevKit V1)**.

The proposed configuration uses:

-   one SPI bus shared by all readers;
-   a dedicated **SS (Slave Select)** pin for each module;
-   one shared **RST (Reset)** pin for all modules;
-   a function for performing a global hardware reset;
-   a function for initializing all readers;
-   a `restart()` function that combines the two operations.

The goal is to keep hardware topology and coordination in the sketch,
while each `RC522_Driver` instance manages its own reader.

> **Note:** The pin assignments in this guide are examples for the
> classic ESP32. Always check your board's pinout. ESP32-S2, S3, C3, and
> other variants may have different pin assignments and restrictions.

## 2. Hardware Connections

All MFRC522 modules share the SPI signal lines. Each module must have
its own SS pin.

### Example pin assignment

  Signal             ESP32 MFRC522 modules
  -------------- --------- ---------------------------------------------
  SCK              GPIO 18 SCK on all modules
  MOSI             GPIO 23 MOSI on all modules
  MISO             GPIO 19 MISO on all modules
  Reader 1 SS      GPIO 25 SDA/SS on module 1
  Reader 2 SS      GPIO 27 SDA/SS on module 2
  Reader 3 SS      GPIO 32 SDA/SS on module 3
  Shared RESET     GPIO 26 RST on all modules
  Power              3.3 V VCC, according to the module specifications
  Ground               GND GND on all modules

GPIO 18, 19, and 23 are commonly used as the VSPI pins on the classic
ESP32. If your project uses a different SPI configuration, adapt the
wiring and SPI bus initialization accordingly.

### Wiring rules

-   Connect SCK, MOSI, and MISO in parallel to all readers.
-   Connect each reader's SS input to a different GPIO.
-   Connect all module RST pins together and wire them to the shared
    reset GPIO.
-   Connect all GND pins together.
-   Use logic levels compatible with the ESP32: its GPIOs operate at 3.3
    V.
-   Check the power requirements of the specific MFRC522 breakout board.
    Do not assume that every board accepts 5 V on its power or signal
    pins.

When a reader is selected, its SS pin is driven LOW. The SS pins of all
other readers must remain HIGH. The library and SPI configuration must
ensure that device selection is handled correctly.

## 3. Sketch Configuration

Keep the hardware configuration in one place in the sketch:

``` cpp
#include "RC522_Driver.h"

// Shared RESET pin for all modules
constexpr uint8_t RST_SHARED = 26;

// Individual SS pins
constexpr uint8_t SS_PINS[] = {
    25,  // Reader 1
    27,  // Reader 2
    32   // Reader 3
};

// Total number of configured readers
constexpr size_t NUM_READERS =
    sizeof(SS_PINS) / sizeof(SS_PINS[0]);

// One driver instance for each reader
RC522_Driver* readers[NUM_READERS] = {};
```

In this example, each driver object is constructed with its own SS pin
and without a dedicated RESET pin:

``` cpp
readers[i] = new RC522_Driver(SS_PINS[i]);
```

Hardware reset is managed by the sketch through `RST_SHARED`.

## 4. Why Keep hardReset() and start() Separate?

With a shared RESET connection, it is important to distinguish the
physical reset of the chips from their software initialization.

### 4.1 `hardReset()`: Global hardware reset

The `hardReset()` function directly controls the ESP32 GPIO pins:

1.  Configures all SS pins as outputs.
2.  Drives all SS pins HIGH to deselect every reader.
3.  Configures the shared RESET pin as an output.
4.  Drives RESET LOW to generate a hardware reset.
5.  Drives RESET HIGH again.
6.  Waits before starting SPI communication.

The reset is **global**: because all modules share the same RST line,
they are reset at the same time.

Example:

``` cpp
void hardReset() {

    // Deselect all readers
    for (size_t i = 0; i < NUM_READERS; i++) {
        pinMode(SS_PINS[i], OUTPUT);
        digitalWrite(SS_PINS[i], HIGH);
    }

    // Shared hardware reset
    pinMode(RST_SHARED, OUTPUT);
    digitalWrite(RST_SHARED, LOW);
    delay(10);

    digitalWrite(RST_SHARED, HIGH);
    delay(50);
}
```

The delays shown are conservative example values. Verify the required
timing against the chip datasheet and the characteristics of the module
being used.

**Why is this important?** An initial hardware reset puts all chips into
a common reset state before the program configures them. It is also
useful when the entire group of readers needs to be recovered and
returned to a known hardware state.

### 4.2 `start()`: Software initialization of all readers

After the hardware reset, each module must be initialized through its
own driver instance.

The `start()` function iterates over the readers array and calls
`begin()` on each object:

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

The call:

``` cpp
readers[i]->begin();
```

is where the individual module is initialized.

The loop calls `begin()` once for each valid reader. `begin()` does not
replace the shared hardware reset: it performs the software
initialization procedure implemented by the driver.

> In this library, `begin()` is treated as a function with no return
> value (`void`). Therefore, calling it does not by itself prove that
> the chip responded correctly. Additional checks, such as reading the
> version register or running the self-test (if supported by the
> driver), are needed to verify operation.

### 4.3 `restart()`: Coordinated reset and initialization

Once `hardReset()` and `start()` are available, a `restart()` function
can combine them:

``` cpp
void restart() {

    hardReset();
    start();
}
```

This function performs the complete restart procedure for all readers:

1.  Performs the shared hardware reset.
2.  Initializes each module again, one at a time.

It is recommended to use `restart()` during system startup and whenever
the entire group of readers needs to be restarted.

## 5. Creating the Driver Instances

Create the driver objects only once, before calling `restart()`:

``` cpp
void createReaders() {

    for (size_t i = 0; i < NUM_READERS; i++) {

        if (readers[i] == nullptr) {
            readers[i] = new RC522_Driver(SS_PINS[i]);
        }
    }
}
```

Object creation is separate from reset and initialization:

-   `createReaders()` creates the C++ objects;
-   `hardReset()` physically resets the chips;
-   `start()` initializes the driver instances;
-   `restart()` performs the reset and initialization in sequence.

Do not call `createReaders()` on every restart: existing instances
should be reused. On embedded systems, memory usage and allocation
strategy should also be considered. For projects that require it, static
object storage may be preferable.

## 6. Recommended Startup Sequence

A basic startup sequence looks like this:

``` cpp
void setup() {

    Serial.begin(115200);
    delay(500);

    createReaders();

    // Global hardware reset + initialization of all readers
    restart();

    // Diagnostic tests and application logic can start here.
}

void loop() {
}
```

The sequence is:

``` text
ESP32 startup
    |
    v
createReaders()
    |
    v
restart()
    |
    +--> hardReset()
    |       |
    |       +--> all SS pins HIGH
    |       +--> RST LOW
    |       +--> RST HIGH
    |
    +--> start()
            |
            +--> reader 1: begin()
            +--> reader 2: begin()
            +--> reader 3: begin()
    |
    v
Diagnostic tests / application
```

## 7. When Should restart() Be Called?

`restart()` is useful:

-   during startup, to initialize all modules in a coordinated way;
-   after an error condition that requires a hardware reset of the
    entire group;
-   after a hardware interruption or reconfiguration, when all chips
    should be returned to a common initial state.

Because the RESET pin is shared, it is not possible to reset one reader
without also resetting the others. After a shared reset, `begin()` must
be called again for **every** reader.

If `start()` is called on its own, without a preceding hardware reset,
all driver instances are initialized again, but the chips do not receive
a new pulse on the RST pin.

## 8. Diagnostics for Individual Readers

After `restart()`, each module can be checked individually.

Useful checks include:

-   reading the version register (`getVersion()`);
-   running the internal self-test (`selfTest()`), if implemented and
    supported;
-   checking RF settings;
-   checking antenna gain;
-   testing the hardware CRC and comparing it with a software
    calculation.

It is important to distinguish between **initialization attempted** and
**reader verified**. If `begin()` does not return a status, the
diagnostic result must be based on subsequent register reads and tests.

## 9. Limitations and Considerations

-   A shared RESET resets all modules.
-   Each reader must have its own SS pin.
-   All modules share the SPI signal lines.
-   Unselected SS pins must remain HIGH.
-   Bus stability also depends on wiring, power supply, cable length,
    and correct signal handling.
-   The number of usable readers depends on available GPIO pins, memory,
    and application requirements.
-   Pin assignments and SPI bus configuration must be adapted to the
    specific ESP32 board.

## 10. Architecture Summary

  Function            Responsibility
  ------------------- -----------------------------------------------
  `createReaders()`   Creates the `RC522_Driver` instances once
  `hardReset()`       Performs a shared hardware reset of all chips
  `start()`           Calls `begin()` on every valid instance
  `restart()`         Calls `hardReset()` followed by `start()`
  Diagnostics         Verifies each reader individually

This architecture keeps the library simple: the driver manages one
MFRC522 device, while the sketch defines the hardware topology and
coordinates the group of modules.

------------------------------------------------------------------------

**Reference configuration:** Classic ESP32, shared SPI bus, multiple
MFRC522 modules with individual SS pins and a common hardware RESET
line.
