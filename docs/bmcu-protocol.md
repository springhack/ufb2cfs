# BMCU Klipper Protocol Shim

This project uses an ESP32 running Klipper firmware as a protocol adapter, not as a real printer controller.

The ESP32 is expected to expose fake Klipper pins. These pins are not physical GPIOs and must not access ESP32 peripherals. Klipper commands are used only as a transport for BMCU actions.

## Goal

Klipper should be able to run G-code like:

```gcode
BMCU_LOAD CHANNEL=0
BMCU_UNLOAD CHANNEL=3
```

The host side sets fake `output_pin` values for mode and channel, then uses `manual_stepper` with `STOP_ON_ENDSTOP=1` as a standard Klipper wait primitive.

## Fake Pins

The firmware reserves fake pin ids starting at `128`:

| Name | Meaning |
| --- | --- |
| `channel_low` | channel bit 0 |
| `channel_high` | channel bit 1 |
| `channel_mode` | `0` = load, `1` = unload |
| `channel_action_dir_pin` | fake manual stepper dir |
| `channel_action_step_pin` | fake manual stepper step trigger |
| `channel_action_endstop_pin` | fake completion signal |

With two channel bits, only channels `0..3` can be represented:

| Channel | low | high |
| --- | --- | --- |
| 0 | 0 | 0 |
| 1 | 1 | 0 |
| 2 | 0 | 1 |
| 3 | 1 | 1 |

Channel `4` needs another bit or a separate convention.

## Firmware Flow

The host config in `printer.cfg` does this:

1. `SET_PIN` writes `channel_low`, `channel_high`, and `channel_mode`.
2. `MANUAL_STEPPER ... STOP_ON_ENDSTOP=1` starts a blocking wait.
3. The ESP32 fake GPIO layer sees a rising edge on `channel_action_step_pin`.
4. `bmcu_fake_start_action()` is called.
5. Real BMCU work should be added in `klipper_src/src/esp32/bmcu_fake.c`.
6. When the work is complete, `channel_action_endstop_pin` should read high so Klipper continues.

At the moment, `bmcu_fake_start_action()` completes immediately. Replace that placeholder with the real asynchronous load/unload work.

## Important Build Note

`src/compile_time_request.c` and `klipper.dict` are pre-generated in this repository. Adding `DECL_ENUMERATION()` in source is necessary, but the generated identify dictionary must also include the fake pin names for a real Klipper host to accept `printer.cfg`.

If fake pin names are not visible to Klipper, regenerate or update the identify dictionary so the `pin` enumeration contains the names above.
