// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "Ev15118D8.hpp"

namespace module {

void Ev15118D8::init() {
    invoke_init(*p_main);
}

void Ev15118D8::ready() {
    invoke_ready(*p_main);
}

} // namespace module
