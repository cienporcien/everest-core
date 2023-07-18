// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Pionix GmbH and Contributors to EVerest

/*
 This is an implementation for the SLIP serial protocol
*/
#ifndef SLIP_PROTOCOL
#define SLIP_PROTOCOL

#include <everest/logging.hpp>
#include <optional>
#include <stdint.h>
#include "ld-ev.hpp"

#include "crc16.hpp"

namespace slip_protocol {

constexpr int SLIP_START_END_FRAME = 0xC0;
constexpr int SLIP_BROADCAST_ADDR = 0xFF;

constexpr uint16_t SLIP_SIZE_ON_ERROR = 1;

const std::vector<uint8_t> SLIP_ERROR_SIZE_ERROR = {0x0A};
const std::vector<uint8_t> SLIP_ERROR_MALFORMED = {0x0B};
const std::vector<uint8_t> SLIP_ERROR_CRC_MISMATCH = {0x0E};

class SlipProtocol {

public:
    SlipProtocol() = default;
    ~SlipProtocol() = default;

    std::vector<uint8_t> package_single(const uint8_t address, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> package_multi(const uint8_t address, const std::vector<std::vector<uint8_t>>& multi_payload);

    std::vector<uint8_t> unpack(std::vector<uint8_t>& message);

// private:
    
};

} // namespace slip_protocol
#endif // SLIP_PROTOCOL
