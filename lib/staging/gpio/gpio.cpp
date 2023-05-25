// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Pionix GmbH and Contributors to EVerest

#include "gpio.hpp"

#include <system_error>

namespace Everest {

bool Gpio::open(const std::string& chip_name, int line_number, bool _inverted) {
    ready = false;

    if (chip_name != "") {
        try {
            chip.open(chip_name);
            line = chip.get_line(line_number);
            ready = true;
        } catch (const std::system_error& e) {
        } catch (const std::out_of_range& e) {
        }
    }

    inverted = _inverted;

    return ready;
}

bool Gpio::open(const GpioSettings& settings) {
    return open(settings.chip_name, settings.line_number, settings.inverted);
}

void Gpio::close() {
    if (ready) {
        line.release();
    }
}

void Gpio::set(bool value) {
    if (ready) {
        if (inverted) {
            value = !value;
        }
        line.set_value((value ? 1 : 0));
    }
}

bool Gpio::set_output(bool initial_value) {
    if (ready) {
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
            // if we cannot set it to output, deactivate it
            ready = false;
        }
    }

    return ready;
}

bool Gpio::set_input() {
    if (ready) {
        try {
            gpiod::line_request c;
            c.consumer = "EVerest";
            c.request_type = gpiod::line_request::DIRECTION_INPUT;
            c.flags = gpiod::line_request::FLAG_BIAS_PULL_UP;
            line.request(c);
        } catch (const std::system_error& e) {
            ready = false;
        }
    }
    return ready;
}

bool Gpio::read() {
    bool value = false;

    if (ready) {
        try {
            value = line.get_value();
        } catch (const std::system_error& e) {
            ready = false;
        }

        if (inverted) {
            value = !value;
        }
    }

    return value;
}

void Gpio::invert_pin(bool _inverted) {
    inverted = _inverted;
}

bool Gpio::is_ready() {
    return ready;
}

} // namespace Everest
