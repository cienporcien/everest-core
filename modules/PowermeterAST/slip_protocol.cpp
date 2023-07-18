// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Pionix GmbH and Contributors to EVerest

#include "slip_protocol.hpp"

#include <cstring>
#include <everest/logging.hpp>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

#include <sys/select.h>
#include <sys/time.h>

#include <fmt/core.h>

#include "crc16.hpp"

namespace slip_protocol {

static std::string hexdump(std::vector<uint8_t> msg) {
    std::stringstream ss;

    for (auto index : msg) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)index << " ";
    }
    return ss.str();
}

static std::string hexdump_u16(uint16_t msg) {
    std::stringstream ss;

    ss << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)msg;

    return ss.str();
}


std::vector<uint8_t> SlipProtocol::package_single(const uint8_t address, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> vec{};

    // address
    vec.push_back(address);

    // payload
    for (auto payload_byte : payload) {
        if (payload_byte == 0xC0) {         // check for and replace special char 0xC0
            vec.push_back(0xDB);
            vec.push_back(0xDC);
        } else if (payload_byte == 0xDB) {  // check for and replace special char 0xDB
            vec.push_back(0xDB);
            vec.push_back(0xDD);
        } else {                            // otherwise just use normal input
            vec.push_back(payload_byte);
        }
    }

    // CRC16
    uint16_t crc = calculate_xModem_crc16(vec);
    vec.push_back(uint8_t(crc & 0x00FF));         // LSB CRC16
    vec.push_back(uint8_t((crc >> 8) & 0x00FF));  // MSB CRC16
    
    // add start frame to front (can be done only after CRC has been calculated, as start frame is not part of CRC)
    vec.insert(vec.begin(), SLIP_START_END_FRAME);

    // end frame
    vec.push_back(SLIP_START_END_FRAME);

    return vec;
}

std::vector<uint8_t> SlipProtocol::package_multi(const uint8_t address, const std::vector<std::vector<uint8_t>>& multi_payload) {
    std::vector<uint8_t> payload{};

    // concatenate requests
    for (auto multi_payload_part : multi_payload) {
        payload.insert(payload.end(), multi_payload_part.begin(), multi_payload_part.end());
    }

    // ...and return as one long request
    return this->package_single(address, payload);
}

std::vector<uint8_t> SlipProtocol::unpack(std::vector<uint8_t>& message) {
    std::vector<uint8_t> data{};
    uint16_t crc_calc = 0;
    uint16_t crc_check = 0xFFFF;

    if (message.size() < 1) {
        return SLIP_ERROR_SIZE_ERROR;
    }
    if (message.at(0) != SLIP_START_END_FRAME) {
        return SLIP_ERROR_MALFORMED; 
    } else {
        message.erase(message.begin());
        crc_check = uint16_t( (message[message.size() - 2] << 8) | message[message.size() - 3] );
        // remove CRC and end frame
        for (uint8_t i = 0; i < 3; i++) {
            message.pop_back();
        }
    }

    crc_calc = calculate_xModem_crc16(message);
    if (crc_check == crc_calc) {
        // message intact, check for special characters and restore to original contents
        for (uint16_t j = 0; j < (message.size() - 1); j++) {  // can only go to message.size() - 1 because two bytes will be checked
            if ((message.at(j) == 0xDB) && (message.at(j + 1) == 0xDC)) {
                message.at(j) = 0xC0;
                message.erase(message.begin() + j + 1);
            } else if ((message.at(j) == 0xDB) && (message.at(j + 1) == 0xDD)) {
                message.at(j) = 0xDB;
                message.erase(message.begin() + j + 1);
            }
        }
        return message;
    } else {
        data = SLIP_ERROR_CRC_MISMATCH;
    }

    return data;
}

} // namespace slip_protocol
