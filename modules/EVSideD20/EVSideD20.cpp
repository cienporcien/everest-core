// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "EVSideD20.hpp"

namespace module {

void EVSideD20::init() {
    invoke_init(*p_ev);
}

void EVSideD20::ready() {
    invoke_ready(*p_ev);
}

} // namespace module
