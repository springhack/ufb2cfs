#include "gpio/gpio.h"
#include "command.h"

DECL_ENUMERATION_RANGE("pin", "GPIO0", GPIO_NUM_0, GPIO_NUM_MAX);
DECL_ENUMERATION("pin", "channel_low", BMCU_FAKE_PIN_CHANNEL_LOW);
DECL_ENUMERATION("pin", "channel_high", BMCU_FAKE_PIN_CHANNEL_HIGH);
DECL_ENUMERATION("pin", "channel_mode", BMCU_FAKE_PIN_CHANNEL_MODE);
DECL_ENUMERATION("pin", "channel_action_dir_pin", BMCU_FAKE_PIN_ACTION_DIR);
DECL_ENUMERATION("pin", "channel_action_step_pin", BMCU_FAKE_PIN_ACTION_STEP);
DECL_ENUMERATION("pin", "channel_action_endstop_pin", BMCU_FAKE_PIN_ACTION_ENDSTOP);
