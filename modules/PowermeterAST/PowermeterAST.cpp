// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "PowermeterAST.hpp"

namespace module {

void PowermeterAST::init() {
    invoke_init(*p_main);
}

void PowermeterAST::ready() {
    invoke_ready(*p_main);
}

} // namespace module
