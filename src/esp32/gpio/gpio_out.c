#include "gpio/gpio_out.h"
#include "bmcu_fake.h"
#include "gpio/shift_register.h"
#include "command.h"
#include "driver/gpio.h"

struct gpio_out gpio_out_setup(gpio_num_t pin, uint_fast8_t val)
{
    if (bmcu_fake_is_output_pin(pin)) {
        bmcu_fake_setup_out(pin, val);
        return (struct gpio_out){ .pin = pin };
    }

    struct gpio_out gpio = { .pin = pin };
    gpio_out_reset(gpio, val);
    return gpio;
}

void gpio_out_reset(struct gpio_out gpio, uint_fast8_t val)
{
    if (bmcu_fake_is_output_pin(gpio.pin)) {
        bmcu_fake_setup_out(gpio.pin, val);
        return;
    }

#if CONFIG_HAVE_GPIO_SR
    if (gpio_is_sr(gpio)) {
        gpio_sr_write(gpio, val);
        return;
    }
#endif

    if (gpio.pin >= GPIO_NUM_MAX) {
        shutdown("Output pin outside of range");
    }

    // We also need to keep the input enabled to be able to read it when toggling
    gpio_pullup_dis(gpio.pin);
    gpio_ll_input_enable(GPIO_LL_GET_HW(GPIO_PORT_0), gpio.pin);

    gpio_out_write(gpio, val);
    gpio_ll_output_enable(GPIO_LL_GET_HW(GPIO_PORT_0), gpio.pin);
}
