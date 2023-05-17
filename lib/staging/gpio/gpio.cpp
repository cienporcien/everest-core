// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Pionix GmbH and Contributors to EVerest

#include "gpio.hpp"

#include <system_error>

namespace Everest {

bool Gpio::open(const std::string& chip_name, int line_number, bool _inverted) {

    if (chip_name == "") {
        is_open = false;
    } else {
        try {
            chip.open(chip_name);
            line = chip.get_line(line_number);
            is_open = true;
        } catch (const std::system_error& e) {
            is_open = false;
        } catch (const std::out_of_range& e) {
            is_open = false;
        }
    }

    inverted = _inverted;

    return is_open;
}

bool Gpio::open(const GpioSettings& settings) {
    return open(settings.chip_name, settings.line_number, settings.inverted);
}

void Gpio::close() {
    if (is_open) {
        line.release();
    }
}

void Gpio::set(bool value) {
    if (!is_open)
        return;

    if (inverted) {
        value = !value;
    }
    line.set_value((value ? 1 : 0));
}

bool Gpio::set_output(bool initial_value) {
    if (!is_open)
        return false;
    if (inverted) {
        initial_value = !initial_value;
    }
    try {
        gpiod::line_request c;
        c.consumer = "EVerest";
        c.request_type = gpiod::line_request::DIRECTION_OUTPUT;
        c.flags = 0;
        line.request(c);
    } catch (const std::system_error& e) {
        is_open = false;
        return false;
    }

    return true;
}

bool Gpio::set_input() {
    if (!is_open)
        return false;
    try {
        gpiod::line_request c;
        c.consumer = "EVerest";
        c.request_type = gpiod::line_request::DIRECTION_INPUT;
        c.flags = gpiod::line_request::FLAG_BIAS_PULL_UP;
        line.request(c);
    } catch (const std::system_error& e) {
        is_open = false;
        return false;
    }
    return true;
}

bool Gpio::read() {
    if (!is_open)
        return false;

    bool value = false;
    try {
        value = line.get_value();
    } catch (const std::system_error& e) {
        is_open = false;
    }

    if (inverted) {
        value = !value;
    }
    return value;
}

void Gpio::invert_pin(bool _inverted) {
    inverted = _inverted;
}

bool Gpio::is_ready() {
    return is_open;
}

} // namespace Everest
