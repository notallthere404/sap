# SAP Architecture

## App State

The application state is an enum which defines the rules and roles for each component at each stage in the runtime.

### Runtime Execution Model

```mermaid
flowchart TD
BOOT[Boot] --> INIT[Initialize core modules]

    INIT --> DIODE_INIT[Init diode PWM]
    INIT --> BUTTON_INIT[Init input buttons]
    INIT --> APP_INIT[Initialize app state]

    DIODE_INIT --> SET_STANDBY
    BUTTON_INIT --> SET_STANDBY
    APP_INIT --> SET_STANDBY[app_set_state APP_STATE_STANDBY]

    SET_STANDBY --> STATE_MACHINE[Central app state machine]

    INPUT[Input subsystem callback] --> APP_EVENT[Translate button event]
    APP_EVENT --> STATE_MACHINE

    BLE_EVT[BLE callbacks] --> STATE_MACHINE
    SENSOR_EVT[Sensor read result] --> STATE_MACHINE
    ERROR_EVT[Component error] --> STATE_MACHINE

    STATE_MACHINE --> DIODE[Update diode display]
    STATE_MACHINE --> SENSOR[Start/stop sensor work]
    STATE_MACHINE --> BLE[Start/stop BLE behavior]
```

### Standby

**Type:** Steady state

| Component    | Allowed Action      |
| ------------ | ------------------- |
| Red Diode    | Dim                 |
| Yellow Diode | Off                 |
| Green Diode  | Off                 |
| Power Button | Short press to wake |
| Pair Button  | Off                 |
| Sensor       | Off                 |
| Bluetooth    | Off                 |

**Process Loop:**

```mermaid
flowchart TD
    ENTER[Enter Standby] --> LED[Set red diode dim or solid]
    LED --> STOP_SENSOR[Stop sensor sampling]
    STOP_SENSOR --> STOP_BLE[Stop BLE advertising/transmission]
    STOP_BLE --> WAIT[Wait for input event]

    WAIT -->|Power long press| WAKE[app_set_state APP_STATE_WAKING]
    WAIT -->|Other input| WAIT
```

### Waking

**Type:** Transition state

| Component    | Action                         |
| ------------ | ------------------------------ |
| Red Diode    | Off                            |
| Yellow Diode | Blinking                       |
| Green Diode  | Off                            |
| Power Button | Long press to abort to standby |
| Pair Button  | Ignored during init            |
| Sensor       | Initializing                   |
| Bluetooth    | Initializing                   |

**Process Execution:**

```mermaid
flowchart TD
    ENTER[Enter Waking] --> LED[Start yellow blink]
    LED --> INIT_SENSOR[Initialize sensors]
    INIT_SENSOR --> SENSOR_OK{Sensor init OK?}

    SENSOR_OK -->|No| ERROR[app_set_state APP_STATE_ERROR]
    SENSOR_OK -->|Yes| INIT_BLE[Initialize BLE stack/services]

    INIT_BLE --> BLE_OK{BLE init OK?}
    BLE_OK -->|No| ERROR
    BLE_OK -->|Yes| READY[app_set_state APP_STATE_BACKGROUND]

    ENTER -->|Power long press abort| ABORT[app_set_state APP_STATE_STANDBY]
```

### Background

**Type:** Steady state

| Component    | Action                 |
| ------------ | ---------------------- |
| Red Diode    | Off                    |
| Yellow Diode | On                     |
| Green Diode  | Off                    |
| Power Button | Long press to standby  |
| Pair Button  | Long press to pairing  |
| Sensor       | Periodic reading       |
| Bluetooth    | Ready, not advertising |

**Process Loop:**

```mermaid
flowchart TD
    ENTER[Enter Background] --> LED[Set yellow diode on]
    LED --> SENSOR_START[Start periodic sensor sampling]
    SENSOR_START --> BLE_READY[BLE initialized but not advertising]
    BLE_READY --> LOOP[Background event loop]

    LOOP --> SENSOR_TICK[Sensor work fires]
    SENSOR_TICK --> READ[Read BME280/VEML7700]
    READ --> READ_OK{Read OK?}

    READ_OK -->|Yes| STORE[Store latest reading]
    STORE --> LOOP

    READ_OK -->|No| ERROR[app_set_state APP_STATE_ERROR]

    LOOP -->|Pair button long press| PAIRING[app_set_state APP_STATE_PAIRING]
    LOOP -->|Power button long press| STANDBY[app_set_state APP_STATE_STANDBY]
```

### Pairing

**Type:** Transition state

| Component    | Action                    |
| ------------ | ------------------------- |
| Red Diode    | Off                       |
| Yellow Diode | Off                       |
| Green Diode  | Blinking                  |
| Power Button | Long press to standby     |
| Pair Button  | Short press to background |
| Sensor       | Periodic reading          |
| Bluetooth    | Advertising               |

**Process Execution:**

```mermaid
flowchart TD
    ENTER[Enter Pairing] --> LED[Start green blink]
    LED --> SENSOR_KEEP[Keep sensor sampling active]
    SENSOR_KEEP --> ADV_START[Start BLE advertising]

    ADV_START --> WAIT[Wait for BLE or input event]

    WAIT -->|Central connects| CONNECTED[BLE connected]
    CONNECTED --> PAIRED[app_set_state APP_STATE_PAIRED]

    WAIT -->|Pair button short press| ABORT[Stop advertising]
    ABORT --> BACKGROUND[app_set_state APP_STATE_BACKGROUND]

    WAIT -->|Power button long press| STOP_ADV[Stop advertising]
    STOP_ADV --> STANDBY[app_set_state APP_STATE_STANDBY]

    WAIT -->|Advertising timeout| TIMEOUT[Stop advertising]
    TIMEOUT --> BACKGROUND

    WAIT -->|BLE error| ERROR[app_set_state APP_STATE_ERROR]
```

### Paired

**Type:** Steady state

| Component    | Action                                      |
| ------------ | ------------------------------------------- |
| Red Diode    | Off                                         |
| Yellow Diode | Off                                         |
| Green Diode  | On                                          |
| Power Button | Long press to standby                       |
| Pair Button  | Short press to disconnect/background        |
| Sensor       | Periodic reading                            |
| Bluetooth    | Connected; transmits when subscribed/needed |

**Process Loop:**

```mermaid
flowchart TD
    ENTER[Enter Paired] --> LED[Set green diode on]
    LED --> SENSOR_START[Ensure sensor sampling active]
    SENSOR_START --> BLE_TX_READY[Enable BLE notifications/transmission]
    BLE_TX_READY --> LOOP[Paired event loop]

    LOOP --> SENSOR_TICK[Sensor work fires]
    SENSOR_TICK --> READ[Read sensors]
    READ --> READ_OK{Read OK?}

    READ_OK -->|No| ERROR[app_set_state APP_STATE_ERROR]
    READ_OK -->|Yes| HAS_SUB{Central subscribed?}

    HAS_SUB -->|Yes| NOTIFY[Send BLE notification]
    HAS_SUB -->|No| STORE[Store latest reading only]

    NOTIFY --> LOOP
    STORE --> LOOP

    LOOP -->|Pair button short press| DISC[Disconnect BLE]
    DISC --> BACKGROUND[app_set_state APP_STATE_BACKGROUND]

    LOOP -->|Power button long press| STANDBY[Disconnect and app_set_state APP_STATE_STANDBY]
    LOOP -->|BLE disconnected| BACKGROUND
```

**Roles:**

| Component    | Action                                     |
| ------------ | ------------------------------------------ |
| Red Diode    | Off                                        |
| Yellow Diode | Off                                        |
| Green Diode  | On                                         |
| Power Button | Long press to set standby                  |
| Pair Button  | Short press to set background (disconnect) |
| Sensor       | Reading                                    |
| Bluetooth    | Transmitting                               |

### Error

**Type:** Fault state with optional recovery

**Process Loop:**

```mermaid
flowchart TD
    ENTER[Enter Error] --> LED[Start red blink]
    LED --> CLASSIFY[Classify error source]

    CLASSIFY --> SENSOR_ERR{Sensor error?}
    CLASSIFY --> BLE_ERR{BLE error?}
    CLASSIFY --> FATAL_ERR{Fatal error?}

    SENSOR_ERR -->|Yes| SENSOR_RECOVER[Attempt sensor recovery]
    BLE_ERR -->|Yes| BLE_RECOVER[Attempt BLE recovery]
    FATAL_ERR -->|Yes| WAIT_USER[Wait for power long press]

    SENSOR_RECOVER --> SENSOR_OK{Recovered?}
    BLE_RECOVER --> BLE_OK{Recovered?}

    SENSOR_OK -->|Yes| BACKGROUND[app_set_state APP_STATE_BACKGROUND]
    SENSOR_OK -->|No| WAIT_USER

    BLE_OK -->|Yes| BACKGROUND
    BLE_OK -->|No| WAIT_USER

    WAIT_USER -->|Power long press| STANDBY[app_set_state APP_STATE_STANDBY]
```

**Roles:**

| Component    | Action                  |
| ------------ | ----------------------- |
| Red Diode    | Blinking                |
| Yellow Diode | Off                     |
| Green Diode  | Off                     |
| Power Button | Long press to standby   |
| Pair Button  | Usually ignored         |
| Sensor       | Depends on error source |
| Bluetooth    | Depends on error source |

_TODO: blink at different speeds to represent different error states_
