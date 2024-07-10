// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "CbexiEV.hpp"

namespace module {

void CbexiEV::init() {
    invoke_init(*p_ev);
}

void CbexiEV::ready() {
    invoke_ready(*p_ev);
}

} // namespace module
