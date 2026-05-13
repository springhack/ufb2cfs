# UFB BMCU Klipper Shim

This repository builds an ESP32 Klipper firmware image that acts as a BMCU
protocol adapter. It is not intended to be a general ESP32 printer controller.
Klipper is used as the host-side command transport, while the ESP32 translates
fake Klipper pin operations into UART commands for the external BMCU adapter.

## Repository Layout

```text
.
├── src/                   All firmware C/H sources and ESP-IDF component files
│   ├── CMakeLists.txt     ESP-IDF component definition
│   └── compile_time_request.c
├── config/
│   ├── box.cfg            Creality/K2 CFS box config; keep as upstream input
│   └── bmcu.cfg           BMCU host config and G-code macros
├── klipper.dict           Generated identify dictionary used by Klipper host
├── platformio.ini         PlatformIO build configuration
├── sdkconfig.defaults     ESP-IDF defaults
└── sdkconfig.esp32        ESP-IDF resolved config
```

`docs/` was intentionally folded into this README so the protocol, build notes,
and maintenance notes live in one place.

## Important Paths

- `src/esp32/bmcu_fake.c`: BMCU fake pin state machine, UART bridge, LED blink, and RX polling.
- `src/esp32/gpio/gpio.c`: Klipper pin enumeration. Only fake BMCU pins should be exposed.
- `src/esp32/gpio/gpio.h`: fake pin id constants.
- `src/CMakeLists.txt`: main ESP-IDF component; includes both generated request code and Klipper MCU sources.
- `src/compile_time_request.c`: generated Klipper request/identify data, manually kept in this repository.
- `config/bmcu.cfg`: host macros `BMCU_LOAD`, `BMCU_UNLOAD`, T0-T7 routing, and internal action phases.
- `config/box.cfg`: original box-side macros including `M8200`; avoid editing unless intentionally updating upstream box behavior.

## Host-Side G-code

The intended public commands are:

```gcode
BMCU_LOAD CHANNEL=0
BMCU_UNLOAD CHANNEL=3
BMCU_UNLOAD
```

`BMCU_UNLOAD` without `CHANNEL` runs channels `0..3` sequentially.

The macro flow is:

1. `BMCU_ACTION_PREPARE` writes fake register pins for channel and mode.
2. `BMCU_ACTION_TRIGGER` pulses `channel_action_trigger_pin` using 100 ms dwells.
3. `BMCU_ACTION_WAIT` runs a fake `manual_stepper` move with `STOP_ON_ENDSTOP=1`.

The 100 ms `G4` dwells keep Klipper output pin events on separate print times.
Without this, repeated `SET_PIN` values may be coalesced or discarded by
Klipper's output pin scheduling.

## K2 CFS And UFB Material Routing

`config/box.cfg` includes `config/bmcu.cfg`, and `config/bmcu.cfg` overrides
the built-in tool commands with `rename_existing: Tn.233`.

Material ownership:

| Tool | Device |
| --- | --- |
| `T0`..`T3` | K2 CFS / box |
| `T4`..`T7` | UFB2CFS / BMCU channels `0`..`3` |

`BMCU_MATERIAL_STATE.current` tracks the selected material as `-1` or `0..7`.
After a Klipper restart, set it manually if filament is already loaded:

```gcode
BMCU_SET_CURRENT_MATERIAL MATERIAL=5
```

The important integration rule is that `BMCU_LOAD` replaces only the physical
`M8200 L` load operation for UFB-owned materials. All other box-side operations
remain part of the T command flow.

Current `BMCU_SELECT_MATERIAL` flow:

1. If a material is already selected, run `M8200 P S{target}`.
2. Cut with `M8200 C S0`.
3. Unload the previous material:
   - previous `0..3`: `M8200 R I{current}`
   - previous `4..7`: `BMCU_UNLOAD CHANNEL={current - 4}`
4. Load the target material:
   - target `0..3`: `M8200 L I{target}`, then `M8200 F`, then `M8200 O S{target}`
   - target `4..7`: set the `M8200.tnn` variable, run `BMCU_LOAD CHANNEL={target - 4}`, then `M8200 W`, `M8200 F`, and `M8200 O S{target}`

`M8200 F` depends on `M8200.tnn`. Because the UFB branch skips `M8200 L`, the
macro `BMCU_SET_M8200_TNN` must set the same TNN value before `M8200 F`.

From the visible `box.cfg` macro layer, `M8200 C` expands only to
`CR_BOX_CUT`; retreat/unload is represented separately by `M8200 R` /
`CR_BOX_RETRUDE`. `CR_BOX_CUT` itself is not implemented in this repository, so
any hidden low-level behavior must be confirmed by printer testing.

## Fake Pins

The ESP32 firmware reserves fake pin ids starting at `128`.

| Name | Id | Direction | Meaning |
| --- | ---: | --- | --- |
| `channel_low` | 128 | output | channel bit 0 |
| `channel_high` | 129 | output | channel bit 1 |
| `channel_mode` | 130 | output | `0` = load, `1` = unload |
| `channel_action_dir_pin` | 131 | output | fake manual stepper dir, accepted but ignored |
| `channel_action_step_pin` | 132 | output | fake manual stepper step, accepted but ignored |
| `channel_action_endstop_pin` | 133 | input | fake completion endstop, returns `!action_active` |
| `channel_action_trigger_pin` | 134 | output | event input; `VALUE=1` starts action if inactive |

Real ESP32 `GPIO0..GPIO39` are deliberately not exposed in the Klipper pin
enumeration. If `printer.cfg` accidentally references `bmcu:GPIO2`, Klipper
should reject it instead of configuring a physical ESP32 pin.

Channel encoding:

| Channel | `channel_low` | `channel_high` |
| ---: | ---: | ---: |
| 0 | 0 | 0 |
| 1 | 1 | 0 |
| 2 | 0 | 1 |
| 3 | 1 | 1 |

Only channels `0..3` are representable with the current two-bit encoding.

## ESP32 State Machine

The firmware intentionally keeps very little fake pin state:

- `channel_low`, `channel_high`, and `channel_mode` are saved because they are
  needed when trigger arrives.
- `action_active` is the single action state.
- `channel_action_trigger_pin` is not saved. `VALUE=1` is treated as an event.
- `channel_action_dir_pin` and `channel_action_step_pin` are not saved.
- `channel_action_endstop_pin` is not saved; reads return `!action_active`.

Action flow:

1. ESP32 boots and initializes BMCU UART.
2. `bmcu_fake_task()` continuously polls RX.
3. Host writes channel/mode fake pins.
4. Host writes `channel_action_trigger_pin = 1`.
5. If inactive, ESP32 sends the external UART command and sets `action_active = 1`.
6. External adapter replies with a line containing `DONE`.
7. On any `DONE`, ESP32 unconditionally sets `action_active = 0`.
8. Klipper's manual stepper endstop read sees inactive and completes.

This design makes `DONE` a state synchronization point. It does not depend on
the ESP32 and Klipper agreeing on the previous action state.

## External BMCU UART

The ESP32 talks to the external BMCU adapter through UART2:

| Signal | ESP32 pin |
| --- | --- |
| BMCU UART TX | GPIO27 / D27 |
| BMCU UART RX | GPIO26 / D26 |

Serial format:

```text
115200 8N1
```

Load command:

```text
INPUT <channel>
```

Unload command:

```text
OUTPUT <channel>
```

The external adapter must reply with exactly:

```text
DONE
```

Line endings may include `\r\n`; `\r` is ignored and `\n` finishes a line.
During an active action, the ESP32 board LED on GPIO2 blinks.

## Generated Identify Data

`src/compile_time_request.c` and `klipper.dict` are generated-style artifacts
kept in the repository because this PlatformIO project does not run Klipper's
normal `scripts/buildcommands.py` pipeline.

When changing any fake pin name, command declaration, `DECL_INIT`, or
`DECL_TASK`, keep all of these in sync:

1. source declarations in `src/...`
2. `klipper.dict`
3. compressed identify blob in `src/compile_time_request.c`
4. call lists in `src/compile_time_request.c`

Important current manual call-list entries:

- `ctr_run_initfuncs()` must call `bmcu_fake_init()`.
- `ctr_run_taskfuncs()` must call `bmcu_fake_task()`.

If these are missing, the source may compile but UART will not be initialized at
boot and RX will not be continuously polled.

## Build And Flash

Build:

```sh
env PLATFORMIO_CORE_DIR=$PWD/.pio pio run -e esp32
```

Flash:

```sh
env PLATFORMIO_CORE_DIR=$PWD/.pio pio run -e esp32 -t upload
```

PlatformIO output lives in `.pio/`, which is ignored and can be deleted. A clean
build will recreate it and re-download ESP32 tools if needed.

## Design Constraints

- Do not modify Klipper host Python code.
- Keep the ESP32 firmware dedicated to the BMCU shim.
- Do not expose physical ESP32 GPIOs to Klipper config.
- Treat `config/box.cfg` as upstream box config; prefer BMCU integration edits in `config/bmcu.cfg`.
- Keep trigger as an event, not as stored pin state.
- Keep endstop as a read of `!action_active`, not as a separate done latch.
- Treat UART `DONE` as unconditional state convergence.

## Notes For Future Code Changes

Before changing behavior, check these details:

- `config/bmcu.cfg` uses `BMCU_RUN`, not the old `BMCU_ACTION` macro.
- The action phases are `BMCU_ACTION_PREPARE`, `BMCU_ACTION_TRIGGER`, and `BMCU_ACTION_WAIT`.
- `BMCU_ACTION_WAIT` resets manual stepper position before the fake move:
  `MANUAL_STEPPER STEPPER=bmcu_channel_action SET_POSITION=0`.
- The fake wait move is currently `MOVE=900 SPEED=10 STOP_ON_ENDSTOP=1`, which
  means a worst-case 90 second Klipper timeout if the endstop never triggers.
- `channel_mode = 0` maps to `INPUT`, which is load.
- `channel_mode = 1` maps to `OUTPUT`, which is unload.
- `action_active` means the ESP32 believes an external BMCU action is currently running.
- Re-entry is blocked in firmware by only starting on trigger when inactive.
- If host macros remain strictly sequential, action re-entry should not occur.

The original upstream Klipper source used for behavior inspection was placed at:

```text
/Users/springhack/Public/Codes/Personal/klipper
```

Relevant Klipper observations:

- `output_pin.py` discards same-value writes.
- Klipper can coalesce output pin updates at the same print time.
- `G4` advances print time and separates the trigger pulse writes.
- `manual_stepper STOP_ON_ENDSTOP=1` uses the endstop read path as the wait condition.

## Current Expected Behavior

After flashing this firmware and loading `printer.cfg`:

1. ESP32 starts polling UART RX at boot.
2. `BMCU_LOAD CHANNEL=3` sends `INPUT 3`.
3. `BMCU_UNLOAD CHANNEL=3` sends `OUTPUT 3`.
4. When external UART returns `DONE`, ESP32 becomes inactive.
5. The fake endstop returns high and Klipper continues.
6. Physical ESP32 GPIO names are unavailable to Klipper configuration.
