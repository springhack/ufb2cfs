#pragma once

#include <stdint.h>

uint_fast8_t bmcu_fake_is_pin(uint32_t pin);
uint_fast8_t bmcu_fake_is_output_pin(uint32_t pin);
uint_fast8_t bmcu_fake_is_input_pin(uint32_t pin);
void bmcu_fake_setup_out(uint32_t pin, uint_fast8_t value);
void bmcu_fake_setup_in(uint32_t pin, int_fast8_t pull_up);
void bmcu_fake_write(uint32_t pin, uint_fast8_t value);
uint_fast8_t bmcu_fake_read(uint32_t pin);
void bmcu_fake_toggle(uint32_t pin);
