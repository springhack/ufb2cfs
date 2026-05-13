#include "bmcu_fake.h"
#include "board/misc.h"
#include "gpio/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "hal/gpio_ll.h"
#include "hal/uart_ll.h"
#include "soc/uart_struct.h"

#include <stdio.h>
#include <string.h>

#define BMCU_UART_NUM UART_NUM_2
#define BMCU_UART_DEV (UART_LL_GET_HW(BMCU_UART_NUM))
#define BMCU_UART_BAUD 115200
#define BMCU_UART_TX_PIN 27
#define BMCU_UART_RX_PIN 26
#define BMCU_DONE_LINE "DONE"
#define BMCU_RX_LINE_MAX 16
#define BMCU_ACTION_LED_PIN GPIO_NUM_2
#define BMCU_ACTION_LED_BLINK_TICKS timer_from_us(250000)

struct bmcu_fake_state {
    uint8_t channel_low;
    uint8_t channel_high;
    uint8_t channel_mode;
    uint8_t action_dir;
    uint8_t action_step;
    uint8_t action_active;
    uint8_t action_done;
    uint8_t uart_initialized;
    uint8_t rx_len;
    char rx_line[BMCU_RX_LINE_MAX];
    uint8_t led_initialized;
    uint8_t led_state;
    uint32_t led_next_toggle;
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
bmcu_fake_led_init(void)
{
    if (bmcu.led_initialized)
        return;

    gpio_ll_output_enable(GPIO_LL_GET_HW(GPIO_PORT_0), BMCU_ACTION_LED_PIN);
    gpio_ll_set_level(GPIO_LL_GET_HW(GPIO_PORT_0), BMCU_ACTION_LED_PIN, 0);
    bmcu.led_initialized = 1;
}

static void
bmcu_fake_led_set(uint_fast8_t value)
{
    bmcu_fake_led_init();
    bmcu.led_state = !!value;
    gpio_ll_set_level(
        GPIO_LL_GET_HW(GPIO_PORT_0), BMCU_ACTION_LED_PIN, bmcu.led_state);
}

static void
bmcu_fake_led_task(void)
{
    if (!bmcu.action_active) {
        if (bmcu.led_state)
            bmcu_fake_led_set(0);
        return;
    }

    uint32_t now = timer_read_time();
    if (!timer_is_before(now, bmcu.led_next_toggle)) {
        bmcu_fake_led_set(!bmcu.led_state);
        bmcu.led_next_toggle = now + BMCU_ACTION_LED_BLINK_TICKS;
    }
}

static void
bmcu_fake_uart_init(void)
{
    if (bmcu.uart_initialized)
        return;

    ESP_ERROR_CHECK(uart_param_config(
        BMCU_UART_NUM,
        &(uart_config_t) {
            .baud_rate = BMCU_UART_BAUD,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_APB,
        }
    ));
    ESP_ERROR_CHECK(uart_set_pin(
        BMCU_UART_NUM,
        BMCU_UART_TX_PIN,
        BMCU_UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    uart_ll_rxfifo_rst(BMCU_UART_DEV);
    uart_ll_txfifo_rst(BMCU_UART_DEV);
    uart_ll_disable_intr_mask(BMCU_UART_DEV, UART_LL_INTR_MASK);
    uart_ll_clr_intsts_mask(BMCU_UART_DEV, UART_LL_INTR_MASK);
    uart_ll_set_tx_idle_num(BMCU_UART_DEV, 0);

    bmcu.uart_initialized = 1;
}

static void
bmcu_fake_uart_write(const char *data)
{
    bmcu_fake_uart_init();
    uart_ll_write_txfifo(BMCU_UART_DEV, (const uint8_t *)data, strlen(data));
}

static void
bmcu_fake_finish_rx_line(void)
{
    bmcu.rx_line[bmcu.rx_len] = '\0';
    if (strcmp(bmcu.rx_line, BMCU_DONE_LINE) == 0) {
        bmcu.action_done = 1;
        bmcu.action_active = 0;
    }
    bmcu.rx_len = 0;
}

static void
bmcu_fake_poll_uart(void)
{
    bmcu_fake_led_task();

    if (!bmcu.uart_initialized || !bmcu.action_active)
        return;

    uint8_t data[16];
    while (uart_ll_get_rxfifo_len(BMCU_UART_DEV)) {
        uint32_t len = uart_ll_get_rxfifo_len(BMCU_UART_DEV);
        if (len > sizeof(data))
            len = sizeof(data);
        uart_ll_read_rxfifo(BMCU_UART_DEV, data, len);

        for (uint32_t i = 0; i < len; i++) {
            uint8_t c = data[i];
            if (c == '\r')
                continue;
            if (c == '\n') {
                bmcu_fake_finish_rx_line();
                continue;
            }
            if (bmcu.rx_len < BMCU_RX_LINE_MAX - 1) {
                bmcu.rx_line[bmcu.rx_len++] = c;
            } else {
                bmcu.rx_len = 0;
            }
        }
    }
}

static void
bmcu_fake_start_action(void)
{
    uint8_t channel = bmcu.channel_low | (bmcu.channel_high << 1);
    const char *command = bmcu.channel_mode ? "OUTPUT" : "INPUT";
    char request[16];

    bmcu.action_active = 1;
    bmcu.action_done = 0;
    bmcu.rx_len = 0;
    bmcu.led_next_toggle = timer_read_time() + BMCU_ACTION_LED_BLINK_TICKS;
    bmcu_fake_led_set(1);

    /*
     * The external BMCU adapter starts work after receiving:
     *   INPUT <channel>
     *   OUTPUT <channel>
     * and sends a DONE line when that work is complete.
     */
    snprintf(request, sizeof(request), "%s %u\n", command, channel);
    bmcu_fake_uart_write(request);
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
    bmcu_fake_poll_uart();

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
