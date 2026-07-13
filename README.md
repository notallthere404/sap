# Sap

Sensor Array for Plants (S.A.P) is an embedded project for the nRF52840 DK that utilizes the BME280 and VEML7700 sensors.

## Hardware Prototype

### Components

- nRF52840 DK
- Breadboard
- Jumper wires
- Adafruit BME280 2652
- Adafruit VEML7700 4162
- 2 × momentary push button
- 1 × LED red
- 1 × LED yellow
- 1 × LED green
- 3 × Resistors 470 Ω



### Pin Allocation

| Function          | nRF52840 pin | External label | Notes                                 |
| ----------------- | -----------: | -------------: | ------------------------------------- |
| I2C SDA           |      `P0.26` |          `SDA` | Shared by BME280 and VEML7700         |
| I2C SCL           |      `P0.27` |          `SCL` | Shared by BME280 and VEML7700         |
| Button 1          |      `P1.01` |           `D0` | Active-low input with pull-up         |
| Reserved button 2 |      `P1.02` |           `D1` | Reserved for second active-low button |
| Red LED           |      `P1.03` |           `D2` | PWM-capable GPIO output               |
| Yellow LED        |      `P1.04` |           `D3` | PWM-capable GPIO output               |
| Green LED         |      `P1.05` |           `D4` | PWM-capable GPIO output               |

### Topology

```mermaid
flowchart TD
    DK[nRF52840 DK]

    subgraph BB[Breadboard - sensor bus]
        PWR["+ rail<br/>VDD / ~3.0 V"]
        GND["- rail<br/>GND"]

        PWR46["power rail position 46"]
        GND46["ground rail position 46"]

        PWR49["power rail position 49"]
        GND49["ground rail position 49"]

        VEML_VIN["59F<br/>VEML7700 VIN"]
        VEML_GND["61F<br/>VEML7700 GND"]

        SCL["row 62<br/>SCL bus<br/>62F / 62I / 62J"]
        SDA["row 63<br/>SDA bus<br/>63F / 63I / 63J"]
    end

    VEML[VEML7700<br/>Adafruit 4162<br/>soldered header]
    BME[BME280<br/>Adafruit 2652<br/>Qwiic/STEMMA QT lead]

    DK -- "red<br/>VDD / VDD_nRF / ~3.0 V" --> PWR46
    PWR46 --> PWR

    DK -- "black<br/>GND" --> GND46
    GND46 --> GND

    DK -- "yellow<br/>P0.27 / SCL" --> SCL
    DK -- "blue<br/>P0.26 / SDA" --> SDA

    PWR -- "wire to VIN" --> VEML_VIN
    GND -- "wire to GND" --> VEML_GND
    VEML_VIN -- "VIN" --> VEML
    VEML_GND -- "GND" --> VEML
    SCL -- "62F / SCL" --> VEML
    SDA -- "63F / SDA" --> VEML

    PWR49 --> PWR
    GND49 --> GND
    PWR49 -- "red / VIN" --> BME
    GND49 -- "black / GND" --> BME
    SCL -- "yellow / SCL 62I" --> BME
    SDA -- "blue / SDA 63I" --> BME
```

```mermaid
flowchart TD
    DK[nRF52840 DK]

    subgraph BB[Breadboard - controls and status LEDs]
        GND["- rail<br/>GND"]

        BTN1["Button 1<br/>Power button"]
        BTN2["Reserved Button 2<br/>Pair button"]

        RR["Red LED<br/>series resistor"]
        YR["Yellow LED<br/>series resistor"]
        GR["Green LED<br/>series resistor"]

        RLED["Red LED"]
        YLED["Yellow LED"]
        GLED["Green LED"]
    end

    DK -- "P1.01 / D0" --> BTN1
    BTN1 -- "active-low<br/>press connects to GND" --> GND

    DK -. "P1.02 / D1<br/>reserved" .-> BTN2
    BTN2 -. "active-low<br/>press connects to GND" .-> GND

    DK -- "P1.03 / D2" --> RR
    RR --> RLED
    RLED --> GND

    DK -- "P1.04 / D3" --> YR
    YR --> YLED
    YLED --> GND

    DK -- "P1.05 / D4" --> GR
    GR --> GLED
    GLED --> GND
```

### External Button Wiring

The external button is wired as an active-low input:

```text
P1.01 / D0 ---- button ---- GND
```

The pin should be configured with an internal pull-up:

```dts
gpios = <&gpio1 1 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
```

`P1.02 / D1` is reserved for the same wiring pattern if a second button is added later.

### External LED Wiring

Each external LED must have its own current-limiting resistor:

```text
P1.03 / D2 -> resistor -> red LED -> GND
P1.04 / D3 -> resistor -> yellow LED -> GND
P1.05 / D4 -> resistor -> green LED -> GND
```

---

## Setup

**Overlay**

[Overlay](./boards/nrf52840dk_nrf52840.overlay)

**Config**

[Config](prj.conf)

---

## Testing

### Sensor Test

**I2C Scan**

`i2c scan i2c@40003000`

```bash
00:             -- -- -- -- -- -- -- -- -- -- -- --
10: 10 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- 77
2 devices found on i2c@40003000
```

**Sensor Test**

_1. BME280_

`sensor get bme280@77`

```bash
channel type=13(ambient_temp) index=0 shift=16 num_samples=1 value=14470775997ns (24.899993)
channel type=14(press) index=0 shift=23 num_samples=1 value=14470775997ns (102.027343)
channel type=16(humidity) index=0 shift=21 num_samples=1 value=14470775997ns (76.375976)
```

_2. VEML7700_

`sensor get veml7700@10`

```bash
channel type=18(light) index=0 shift=4 num_samples=1 value=211120086669ns (11.000000)
```

### Button Test

### LED Test
