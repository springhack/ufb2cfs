#pragma once

#include "soc/gpio_num.h"

#define BMCU_FAKE_PIN_BASE 128
#define BMCU_FAKE_PIN_CHANNEL_LOW (BMCU_FAKE_PIN_BASE + 0)
#define BMCU_FAKE_PIN_CHANNEL_HIGH (BMCU_FAKE_PIN_BASE + 1)
#define BMCU_FAKE_PIN_CHANNEL_MODE (BMCU_FAKE_PIN_BASE + 2)
#define BMCU_FAKE_PIN_ACTION_DIR (BMCU_FAKE_PIN_BASE + 3)
#define BMCU_FAKE_PIN_ACTION_STEP (BMCU_FAKE_PIN_BASE + 4)
#define BMCU_FAKE_PIN_ACTION_ENDSTOP (BMCU_FAKE_PIN_BASE + 5)

struct gpio { gpio_num_t pin; };
#define gpio_in gpio
#define gpio_out gpio
