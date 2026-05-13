#include "bmcu_fake.h"
#include "gpio/gpio.h"

struct bmcu_fake_state {
    uint8_t channel_low;
    uint8_t channel_high;
    uint8_t channel_mode;
    uint8_t action_dir;
    uint8_t action_step;
    uint8_t action_active;
    uint8_t action_done;
};

static struct bmcu_fake_state bmcu;

static void
bmcu_fake_arm_action(void)
{
    bmcu.action_active = 0;
    bmcu.action_done = 0;
}

uint_fast8_t
bmcu_fake_is_pin(uint32_t pin)
{
    return pin >= BMCU_FAKE_PIN_CHANNEL_LOW && pin <= BMCU_FAKE_PIN_ACTION_ENDSTOP;
}

uint_fast8_t
bmcu_fake_is_output_pin(uint32_t pin)
{
    return pin >= BMCU_FAKE_PIN_CHANNEL_LOW && pin <= BMCU_FAKE_PIN_ACTION_STEP;
}

uint_fast8_t
bmcu_fake_is_input_pin(uint32_t pin)
{
    return pin == BMCU_FAKE_PIN_ACTION_ENDSTOP;
}

void
bmcu_fake_setup_out(uint32_t pin, uint_fast8_t value)
{
    bmcu_fake_write(pin, value);
}

void
bmcu_fake_setup_in(uint32_t pin, int_fast8_t pull_up)
{
    (void)pin;
    (void)pull_up;
}

static void
bmcu_fake_start_action(void)
{
    bmcu.action_active = 1;
    bmcu.action_done = 0;

    /*
     * Reserved hook for the real BMCU operation.
     *
     * The selected channel is:
     *   channel = channel_low | (channel_high << 1)
     *
     * The selected mode is:
     *   channel_mode == 0 -> load
     *   channel_mode == 1 -> unload
     *
     * Replace the immediate completion below with the real asynchronous work,
     * then set bmcu.action_done once that work is complete.
     */
    bmcu.action_done = 1;
    bmcu.action_active = 0;
}

void
bmcu_fake_write(uint32_t pin, uint_fast8_t value)
{
    uint8_t bit = !!value;

    switch (pin) {
    case BMCU_FAKE_PIN_CHANNEL_LOW:
        bmcu.channel_low = bit;
        bmcu_fake_arm_action();
        break;
    case BMCU_FAKE_PIN_CHANNEL_HIGH:
        bmcu.channel_high = bit;
        bmcu_fake_arm_action();
        break;
    case BMCU_FAKE_PIN_CHANNEL_MODE:
        bmcu.channel_mode = bit;
        bmcu_fake_arm_action();
        break;
    case BMCU_FAKE_PIN_ACTION_DIR:
        bmcu.action_dir = bit;
        break;
    case BMCU_FAKE_PIN_ACTION_STEP:
        if (!bmcu.action_step && bit && !bmcu.action_active && !bmcu.action_done)
            bmcu_fake_start_action();
        bmcu.action_step = bit;
        break;
    }
}

uint_fast8_t
bmcu_fake_read(uint32_t pin)
{
    switch (pin) {
    case BMCU_FAKE_PIN_CHANNEL_LOW:
        return bmcu.channel_low;
    case BMCU_FAKE_PIN_CHANNEL_HIGH:
        return bmcu.channel_high;
    case BMCU_FAKE_PIN_CHANNEL_MODE:
        return bmcu.channel_mode;
    case BMCU_FAKE_PIN_ACTION_DIR:
        return bmcu.action_dir;
    case BMCU_FAKE_PIN_ACTION_STEP:
        return bmcu.action_step;
    case BMCU_FAKE_PIN_ACTION_ENDSTOP:
        return bmcu.action_done;
    }
    return 0;
}

void
bmcu_fake_toggle(uint32_t pin)
{
    bmcu_fake_write(pin, !bmcu_fake_read(pin));
}
