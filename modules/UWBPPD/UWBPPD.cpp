// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "UWBPPD.hpp"

namespace module {

void UWBPPD::init() {
    invoke_init(*p_main);
}

void UWBPPD::ready() {
    invoke_ready(*p_main);
}

} // namespace module
