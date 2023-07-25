// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

#include <date/date.h>
#include <date/tz.h>
#include <everest/logging.hpp>
#include <nlohmann/json.hpp>
#include "ast_app_layer.hpp"

namespace module {

using json = nlohmann::json;

struct OcmfStats {
    uint32_t number_transactions{};
    uint32_t timestamp_first_transaction{};
    uint32_t timestamp_last_transaction{};
    uint32_t max_number_of_transactions{};    // ???
};

struct OcmfInfo {
    std::string gateway_id{};
    std::string manufacturer{};
    std::string model{};
};

struct DeviceData {
    uint32_t utc_time_s{};
    uint8_t gmt_offset_quarterhours{};
    uint64_t total_start_import_energy_Wh{};  // meter value needs to be divided by 10 for Wh
    uint64_t total_stop_import_energy_Wh{};   // meter value needs to be divided by 10 for Wh
    uint64_t total_start_export_energy_Wh{};  // meter value needs to be divided by 10 for Wh
    uint64_t total_stop_export_energy_Wh{};   // meter value needs to be divided by 10 for Wh
    uint32_t total_transaction_duration_s{};  // must be less than 27 days in total
    OcmfStats ocmf_stats;
    std::string last_ocmf_transaction{};
    std::string requested_ocmf{};
    OcmfInfo ocmf_info;
    uint64_t total_dev_import_energy_Wh{};  // meter value needs to be divided by 10 for Wh
    uint64_t total_dev_export_energy_Wh{};  // meter value needs to be divided by 10 for Wh
    uint64_t status{};
};

void to_json(json& j, const DeviceData& k);
void from_json(const json& j, DeviceData& k);
std::ostream& operator<<(std::ostream& os, const DeviceData& k);

struct LogStats {
    uint32_t number_log_entries{};
    uint32_t timestamp_first_log{};
    uint32_t timestamp_last_log{};
    uint32_t max_number_of_logs{};    // ???
};

struct ApplicationBoardInfo {
    std::string type{};
    std::string hw_ver{};
    std::string server_id{};
    uint8_t mode{};
    uint32_t serial_number{};
    std::string sw_ver{};
    uint16_t fw_crc{};
    uint16_t fw_hash{};
};

struct MeteringBoardInfo {
    std::string hw_ver{};
    std::string sw_ver{};
    uint16_t fw_crc{};
};

struct DeviceDiagnostics {
    std::string charge_point_id{};
    LogStats log_stats;
    ApplicationBoardInfo app_board;
    MeteringBoardInfo m_board;
    std::string pubkey_asn1{};
    std::string pubkey_str16{};
    std::string pubkey{};
    uint8_t pubkey_str16_format{};  // 0x04 for uncompressed string
    uint8_t pubkey_format{};        // 0x04 for uncompressed string
    std::vector<uint8_t> ocmf_config_table{};
};

void to_json(json& j, const DeviceDiagnostics& k);
void from_json(const json& j, DeviceDiagnostics& k);
std::ostream& operator<<(std::ostream& os, const DeviceDiagnostics& k);

// TODO(LAD): add error data

struct ErrorData {
    uint32_t id{};
    uint16_t priority{};
    uint32_t counter{};
};

struct FiveErrors {
    ErrorData error[5];
};

struct ErrorSet {
    FiveErrors category[4];
};

struct Logging {
    ast_app_layer::LogEntry last_log;
    ErrorSet source[2];
};

void to_json(json& j, const Logging& k);
void from_json(const json& j, Logging& k);
std::ostream& operator<<(std::ostream& os, const Logging& k);


namespace conversions {

// std::string state_to_string(State e);
// State string_to_state(const std::string& s);

} // namespace conversions
} // namespace module

#endif // DIAGNOSTICS_HPP