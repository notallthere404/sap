# SAP

Sensor Array for Plants (S.A.P) is an embedded application for the nRF52840 DK. It samples ambient conditions with a BME280 (temperature, pressure, humidity) and a VEML7700 (ambient light), signals state on three LEDs, and exposes readings over BLE.

The VEML7700 is driven by a custom out-of-tree Zephyr driver in [drivers/sensor/veml7700/](drivers/sensor/veml7700/). See [docs/architecture.md](docs/architecture.md) for the application state machine and the driver design.

## Hardware

### Components

| Qty | Part                   | Notes               |
| --: | ---------------------- | ------------------- |
|   1 | nRF52840 DK            | Target board        |
|   1 | Adafruit BME280 2652   | I2C, address `0x77` |
|   1 | Adafruit VEML7700 4162 | I2C, address `0x10` |
|   2 | Momentary push button  | Power, Pair         |
|   3 | LED (red/yellow/green) | Status indication   |
|   3 | Resistor 470 Ω         | One per LED, series |
|   1 | Breadboard             |                     |
|   - | Jumper wires           |                     |

### Pin allocation

| Function     | nRF52840 pin | External label | Notes                         |
| ------------ | -----------: | -------------: | ----------------------------- |
| I2C SDA      |      `P0.26` |          `SDA` | Shared by BME280 and VEML7700 |
| I2C SCL      |      `P0.27` |          `SCL` | Shared by BME280 and VEML7700 |
| Power button |      `P1.01` |           `D0` | Active low, internal pull-up  |
| Pair button  |      `P1.02` |           `D1` | Active low, internal pull-up  |
| Red LED      |      `P1.03` |           `D2` | PWM output                    |
| Yellow LED   |      `P1.04` |           `D3` | PWM output                    |
| Green LED    |      `P1.05` |           `D4` | PWM output                    |

### Wiring

Both sensors share one I2C bus (`i2c0`), so `SDA`, `SCL`, power and ground are common to both.

**VEML7700 (Adafruit 4162)**

| Sensor pin | Connects to | Wire   |
| ---------- | ----------- | ------ |
| `VIN`      | `VDD`       | red    |
| `GND`      | `GND`       | black  |
| `SCL`      | `P0.27`     | yellow |
| `SDA`      | `P0.26`     | blue   |

**BME280 (Adafruit 2652)**

| Sensor pin | Connects to | Wire   |
| ---------- | ----------- | ------ |
| `VIN`      | `VDD`       | red    |
| `GND`      | `GND`       | black  |
| `SCL`      | `P0.27`     | yellow |
| `SDA`      | `P0.26`     | blue   |

**Buttons**

Both buttons are active low. The pin idles high through the internal pull-up and is pulled to ground when pressed, so no external resistor is needed.

```text
P1.01 / D0 ---- power button ---- GND
P1.02 / D1 ---- pair button ----- GND
```

Configured in the overlay as:

```dts
gpios = <&gpio1 1 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
```

**LEDs**

Each LED needs its own series resistor. Driven by PWM so brightness can be varied (standby uses a dim red rather than full on).

```text
P1.03 / D2 ---- 470 Ω ---- red LED ----- GND
P1.04 / D3 ---- 470 Ω ---- yellow LED -- GND
P1.05 / D4 ---- 470 Ω ---- green LED --- GND
```

## Build and flash

Board target is `nrf52840dk/nrf52840`.

```bash
west build -b nrf52840dk/nrf52840
west flash
```

Use a pristine build after changing devicetree, Kconfig, or the module layout:

```bash
west build -b nrf52840dk/nrf52840 -p
```

Configuration lives in [prj.conf](prj.conf) and the board overlay in [boards/nrf52840dk_nrf52840.overlay](boards/nrf52840dk_nrf52840.overlay).

Note that `CONFIG_VEML7700=n` and `CONFIG_VEML7700_CUSTOM=y`: the in-tree Vishay driver is disabled so the out-of-tree driver owns the device.

## Bring-up verification

The shell commands below need options that are commented out in [prj.conf](prj.conf) by default. Enable them for a bring-up build:

```conf
CONFIG_SHELL=y
CONFIG_I2C_SHELL=y
CONFIG_SENSOR_SHELL=y
```

### Bus scan

Confirms both sensors acknowledge on the shared bus.

`i2c scan i2c@40003000`

```text
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

`0x10` is the VEML7700, `0x77` is the BME280.

### Sensor read

**BME280**

`sensor get bme280@77`

```text
channel type=13(ambient_temp) index=0 shift=16 num_samples=1 value=14470775997ns (24.899993)
channel type=14(press) index=0 shift=23 num_samples=1 value=14470775997ns (102.027343)
channel type=16(humidity) index=0 shift=21 num_samples=1 value=14470775997ns (76.375976)
```

**VEML7700**

`sensor get veml7700@10`

Reports `type=18(light)` in lux. The custom driver encodes lux as q31 with `shift=18`, so the reported shift differs from older captures taken against the in-tree driver.

> Sample output not yet recaptured against the custom driver. Re-run and paste.

### Button and LED checks

> Not yet documented.
