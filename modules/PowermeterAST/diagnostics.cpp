// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <diagnostics.hpp>

namespace module {

namespace conversions {

// std::string state_to_string(State e) {
//     switch (e) {
//     case State::DISCONNECTED:
//         return "disconnected";
//     case State::CONNECTED:
//         return "connected";
//     }
//     throw std::out_of_range("No known string conversion for provided enum of type State");
// }

// State string_to_state(const std::string& s) {
//     if (s == "disconnected") {
//         return State::DISCONNECTED;
//     }
//     if (s == "connected") {
//         return State::CONNECTED;
//     }
//     throw std::out_of_range("Provided string " + s + " could not be converted to enum of type State");
// }

} // namespace conversions


void to_json(json& j, const DeviceData& k) {
    j["UTC"] = k.utc_time_s;
    EVLOG_error << "1";
    j["GMT_offset_quarterhours"] = k.gmt_offset_quarterhours;
    EVLOG_error << "2";
    j["total_start_import_energy_Wh"] = k.total_start_import_energy_Wh;
    EVLOG_error << "3";
    j["total_stop_import_energy_Wh"] = k.total_stop_import_energy_Wh;
    EVLOG_error << "4";
    j["total_start_export_energy_Wh"] = k.total_start_export_energy_Wh;
    EVLOG_error << "5";
    j["total_stop_export_energy_Wh"] = k.total_stop_export_energy_Wh;
    EVLOG_error << "6";
    j["total_transaction_duration_s"] = k.total_transaction_duration_s;
    EVLOG_error << "7";
    j["OCMF_stats"] = json();
    EVLOG_error << "8";
    j["OCMF_stats"]["number_transactions"] = k.ocmf_stats.number_transactions;
    EVLOG_error << "9";
    j["OCMF_stats"]["timestamp_first_transaction"] = k.ocmf_stats.timestamp_first_transaction;
    EVLOG_error << "10";
    j["OCMF_stats"]["timestamp_last_transaction"] = k.ocmf_stats.timestamp_last_transaction;
	EVLOG_error << "11";
    j["OCMF_stats"]["max_number_of_transactions"] = k.ocmf_stats.max_number_of_transactions;
	EVLOG_error << "12";
    j["last_ocmf_transaction"] = k.last_ocmf_transaction;
	EVLOG_error << "13";
    j["requested_ocmf"] = k.requested_ocmf;
	EVLOG_error << "14";
    j["OCMF_info"] = json();
	EVLOG_error << "15";
    j["OCMF_info"]["gateway_id"] = k.ocmf_info.gateway_id;
	EVLOG_error << "16";
    j["OCMF_info"]["manufacturer"] = k.ocmf_info.manufacturer;
	EVLOG_error << "17";
    j["OCMF_info"]["model"] = k.ocmf_info.model;
	EVLOG_error << "18";
    j["total_dev_import_energy_Wh"] = k.total_dev_import_energy_Wh;
	EVLOG_error << "19";
    j["total_dev_export_energy_Wh"] = k.total_dev_export_energy_Wh;
	EVLOG_error << "20";
    j["status"] = k.status;
}

void from_json(const json& j, DeviceData& k) {
    // k.utc_time_s = j.at("");
    // k.gmt_offset_quarterhours = j.at("");
    // k.total_start_import_energy_Wh = j.at("");
    // k.total_stop_import_energy_Wh = j.at("");
    // k.total_start_export_energy_Wh = j.at("");
    // k.total_stop_export_energy_Wh = j.at("");
    // k.total_transaction_duration_s = j.at("");
    // k.ocmf_stats.number_transactions = j.at("");
    // k.ocmf_stats.timestamp_first_transaction = j.at("");
    // k.ocmf_stats.timestamp_last_transaction = j.at("");
    // k.ocmf_stats.max_number_of_transactions = j.at("");
    // k.last_ocmf_transaction = j.at("");
    // k.ocmf_info.gateway_id = j.at("");
    // k.ocmf_info.manufacturer = j.at("");
    // k.ocmf_info.model = j.at("");
    // k.total_dev_import_energy_Wh = j.at("");
    // k.total_dev_export_energy_Wh = j.at("");
    EVLOG_error << "[DeviceData][from_json()] not implemented";
}

std::ostream& operator<<(std::ostream& os, const DeviceData& k) {
    os << json(k).dump(4);
    return os;
}

void to_json(json& j, const DeviceDiagnostics& k) {
    j["charge_point_id"] = k.charge_point_id;
    j["charge_point_id_type"] = k.charge_point_id_type;
    j["log_stats"] = json();
    j["log_stats"]["number_log_entries"] = k.log_stats.number_log_entries;
    j["log_stats"]["timestamp_first_log"] = k.log_stats.timestamp_first_log;
    j["log_stats"]["timestamp_last_log"] = k.log_stats.timestamp_last_log;
    j["log_stats"]["max_number_of_logs"] = k.log_stats.max_number_of_logs;
    j["app_board"] = json();
    j["app_board"]["type"] = k.app_board.type;
    j["app_board"]["HW_ver"] = k.app_board.hw_ver;
    j["app_board"]["server_id"] = k.app_board.server_id;
    j["app_board"]["mode"] = k.app_board.mode;
    j["app_board"]["serial"] = k.app_board.serial_number;
    j["app_board"]["SW_ver"] = k.app_board.sw_ver;
    j["app_board"]["FW_CRC"] = k.app_board.fw_crc;
    j["app_board"]["FW_hash"] = k.app_board.fw_hash;
    j["m_board"] = json();
    j["m_board"]["SW_ver"] = k.m_board.sw_ver;
    j["m_board"]["FW_CRC"] = k.m_board.fw_crc;
    j["pubkey"] = json();
    j["pubkey"]["asn1"] = json();
    j["pubkey"]["str16"] = json();
    j["pubkey"]["default"] = json();
    j["pubkey"]["asn1"]["key"] = k.pubkey_asn1;
    j["pubkey"]["str16"]["key"] = k.pubkey_str16;
    j["pubkey"]["str16"]["format"] = k.pubkey_str16_format;
    j["pubkey"]["default"]["key"] = k.pubkey;
    j["pubkey"]["default"]["format"] = k.pubkey_format;
    j["ocmf_config_table"] = json::array();
    if(k.ocmf_config_table.size() > 0) {
        for (uint8_t n = 0; n < k.ocmf_config_table.size(); n++) {
            j["ocmf_config_table"][n] = k.ocmf_config_table.at(n);
        }
    }
}

void from_json(const json& j, DeviceDiagnostics& k) {
    // k.charge_point_id = j.at("");
    // k.log_stats.number_log_entries = j.at("");
    // k.log_stats.timestamp_first_log = j.at("");
    // k.log_stats.timestamp_last_log = j.at("");
    // k.log_stats.max_number_of_logs = j.at("");
    // k.app_board.type = j.at("");
    // k.app_board.hw_ver = j.at("");
    // k.app_board.server_id = j.at("");
    // k.app_board.mode = j.at("");
    // k.app_board.serial_number = j.at("");
    // k.app_board.sw_ver = j.at("");
    // k.app_board.fw_crc = j.at("");
    // k.app_board.fw_hash = j.at("");
    // k.m_board.sw_ver = j.at("");
    // k.m_board.fw_crc = j.at("");
    EVLOG_error << "[DeviceDiagnostics][from_json()] not implemented";
}

std::ostream& operator<<(std::ostream& os, const DeviceDiagnostics& k) {
    os << json(k).dump(4);
    return os;
}

void to_json(json& j, const Logging& k) {
    j["log"] = json();
    j["log"]["last"] = json();
    j["log"]["last"]["type"] = k.last_log.type;
    j["log"]["last"]["second_index"] = k.last_log.second_index;
    j["log"]["last"]["utc_time"] = k.last_log.utc_time;
    j["log"]["last"]["utc_offset"] = k.last_log.utc_offset;
    j["log"]["last"]["old_value"] = std::move(std::string(k.last_log.old_value.begin(), k.last_log.old_value.end()));
    j["log"]["last"]["new_value"] = std::move(std::string(k.last_log.new_value.begin(), k.last_log.new_value.end()));
    j["log"]["last"]["server_id"] = std::move(std::string(k.last_log.server_id.begin(), k.last_log.server_id.end()));
    j["log"]["last"]["signature"] = std::move(std::string(k.last_log.signature.begin(), k.last_log.signature.end()));

    j["errors"] = json();
    j["errors"]["system"] = json();
    j["errors"]["system"]["last"] = json::array();
    for (uint8_t n = 0; n < 5; n++) {
        j["errors"]["system"]["last"][n]["id"] = k.source[(uint8_t)ast_app_layer::ErrorSource::SYSTEM].category[(uint8_t)ast_app_layer::ErrorCategory::LAST].error[n].id;
        j["errors"]["system"]["last"][n]["priority"] = k.source[(uint8_t)ast_app_layer::ErrorSource::SYSTEM].category[(uint8_t)ast_app_layer::ErrorCategory::LAST].error[n].priority;
        j["errors"]["system"]["last"][n]["counter"] = k.source[(uint8_t)ast_app_layer::ErrorSource::SYSTEM].category[(uint8_t)ast_app_layer::ErrorCategory::LAST].error[n].counter;
    }
    j["errors"]["system"]["last_critical"] = json::array();
    for (uint8_t n = 0; n < 5; n++) {
        j["errors"]["system"]["last_critical"][n]["id"] = k.source[(uint8_t)ast_app_layer::ErrorSource::SYSTEM].category[(uint8_t)ast_app_layer::ErrorCategory::LAST_CRITICAL].error[n].id;
        j["errors"]["system"]["last_critical"][n]["priority"] = k.source[(uint8_t)ast_app_layer::ErrorSource::SYSTEM].category[(uint8_t)ast_app_layer::ErrorCategory::LAST_CRITICAL].error[n].priority;
        j["errors"]["system"]["last_critical"][n]["counter"] = k.source[(uint8_t)ast_app_layer::ErrorSource::SYSTEM].category[(uint8_t)ast_app_layer::ErrorCategory::LAST_CRITICAL].error[n].counter;
    }
    j["errors"]["communication"] = json();
    j["errors"]["communication"]["last"] = json::array();
    for (uint8_t n = 0; n < 5; n++) {
        j["errors"]["communication"]["last"][n]["id"] = k.source[(uint8_t)ast_app_layer::ErrorSource::COMMUNICATION].category[(uint8_t)ast_app_layer::ErrorCategory::LAST].error[n].id;
        j["errors"]["communication"]["last"][n]["priority"] = k.source[(uint8_t)ast_app_layer::ErrorSource::COMMUNICATION].category[(uint8_t)ast_app_layer::ErrorCategory::LAST].error[n].priority;
        j["errors"]["communication"]["last"][n]["counter"] = k.source[(uint8_t)ast_app_layer::ErrorSource::COMMUNICATION].category[(uint8_t)ast_app_layer::ErrorCategory::LAST].error[n].counter;
    }
    j["errors"]["communication"]["last_critical"] = json::array();
    for (uint8_t n = 0; n < 5; n++) {
        j["errors"]["communication"]["last_critical"][n]["id"] = k.source[(uint8_t)ast_app_layer::ErrorSource::COMMUNICATION].category[(uint8_t)ast_app_layer::ErrorCategory::LAST_CRITICAL].error[n].id;
        j["errors"]["communication"]["last_critical"][n]["priority"] = k.source[(uint8_t)ast_app_layer::ErrorSource::COMMUNICATION].category[(uint8_t)ast_app_layer::ErrorCategory::LAST_CRITICAL].error[n].priority;
        j["errors"]["communication"]["last_critical"][n]["counter"] = k.source[(uint8_t)ast_app_layer::ErrorSource::COMMUNICATION].category[(uint8_t)ast_app_layer::ErrorCategory::LAST_CRITICAL].error[n].counter;
    }
}

void from_json(const json& j, Logging& k) {
    // n/a
    EVLOG_error << "[Logging][from_json()] not implemented";
}

std::ostream& operator<<(std::ostream& os, const Logging& k) {
    os << json(k).dump(4);
    return os;
}

} // namespace module
