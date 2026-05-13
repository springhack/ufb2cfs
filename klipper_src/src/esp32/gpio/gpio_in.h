#pragma once

#include "bmcu_fake.h"
#include "gpio/gpio.h"
#include "hal/gpio_ll.h"

struct gpio_in gpio_in_setup(gpio_num_t pin, int_fast8_t pull_up);
void gpio_in_reset(struct gpio_in gpio, int_fast8_t pull_up);

static inline uint_fast8_t __attribute__((always_inline)) gpio_in_read(struct gpio_in gpio)
{
    if (bmcu_fake_is_input_pin(gpio.pin))
        return bmcu_fake_read(gpio.pin);

    return gpio_ll_get_level(GPIO_LL_GET_HW(GPIO_PORT_0), gpio.pin);
}
