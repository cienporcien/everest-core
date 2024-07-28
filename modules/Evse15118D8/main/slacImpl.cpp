// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "slacImpl.hpp"

namespace module {
namespace main {

void slacImpl::init() {
}

void slacImpl::ready() {
}

void slacImpl::handle_reset(bool& enable) {
    // your code for cmd reset goes here
}

bool slacImpl::handle_enter_bcd() {
    // your code for cmd enter_bcd goes here
    return true;
}

bool slacImpl::handle_leave_bcd() {
    // your code for cmd leave_bcd goes here
    return true;
}

bool slacImpl::handle_dlink_terminate() {
    // your code for cmd dlink_terminate goes here
    return true;
}

bool slacImpl::handle_dlink_error() {
    // your code for cmd dlink_error goes here
    return true;
}

bool slacImpl::handle_dlink_pause() {
    // your code for cmd dlink_pause goes here
    return true;
}

} // namespace main
} // namespace module
