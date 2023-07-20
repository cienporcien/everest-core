// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "powermeterImpl.hpp"

#include <fmt/core.h>

namespace module {
namespace main {

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

void powermeterImpl::init() {
    if (!this->serial_device.open_device(config.serial_port, config.baudrate, config.ignore_echo)) {
        EVLOG_AND_THROW(Everest::EverestConfigError(fmt::format("Cannot open serial port {}.", config.serial_port)));
    }
    this->init_default_values();

    request_device_type();
    set_device_time();
}

void powermeterImpl::ready() {
    std::thread ([this] {
        while (true) {
            read_powermeter_values();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();

    // create device_data publisher thread
    if (this->config.publish_device_data) {
        std::thread ([this] {
            while (true) {
                publish_device_data_topic();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }).detach();
    }

    // create device_diagnostics publisher thread
    if (this->config.publish_device_diagnostics) {
        std::thread ([this] {
            while (true) {
                publish_device_diagnostics_topic();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }).detach();
    }
}

void powermeterImpl::set_device_time() {
    std::chrono::time_point<std::chrono::system_clock> timepoint = std::chrono::system_clock::now();
    int8_t gmt_offset_quarters_of_an_hour = app_layer.get_utc_offset_in_quarter_hours(timepoint);

    std::vector<uint8_t> set_device_time_cmd{};
    app_layer.create_command_set_time(date::utc_clock::from_sys(timepoint), gmt_offset_quarters_of_an_hour, set_device_time_cmd);

    std::vector<uint8_t> slip_msg_set_device_time = std::move(this->slip.package_single(this->config.powermeter_device_id, set_device_time_cmd));

    this->serial_device.tx(slip_msg_set_device_time);
    receive_response();
}

void powermeterImpl::set_device_charge_point_id(ast_app_layer::UserIdType id_type, std::string charge_point_id) {
    std::vector<uint8_t> set_charge_point_id_cmd{};
    app_layer.create_command_set_charge_point_id(id_type, charge_point_id, set_charge_point_id_cmd);
    
    std::vector<uint8_t> slip_msg_set_charge_point_id = std::move(this->slip.package_single(this->config.powermeter_device_id, set_charge_point_id_cmd));

    this->serial_device.tx(slip_msg_set_charge_point_id);
    receive_response();
}

void powermeterImpl::publish_device_data_topic() {
    if (config.publish_device_data) {
        std::string dev_data_topic = this->pm_last_values.meter_id.value() + std::string("/device_data");
        mod->mqtt.publish(dev_data_topic, device_data_obj.to_str());
    }
}

void powermeterImpl::publish_device_diagnostics_topic() {
    if (config.publish_device_diagnostics) {
        std::string dev_diagnostics_topic = this->pm_last_values.meter_id.value() + std::string("/device_diagnostics");
        mod->mqtt.publish(dev_diagnostics_topic, device_diagnostics_obj.to_str());
    }
}

void powermeterImpl::read_device_data() {
    {
        std::vector<uint8_t> get_time_cmd{};
        app_layer.create_command_get_time(get_time_cmd);

        std::vector<uint8_t> get_total_start_import_energy_cmd{};
        app_layer.create_command_get_total_start_import_energy(get_total_start_import_energy_cmd);

        std::vector<uint8_t> get_total_stop_import_energy_cmd{};
        app_layer.create_command_get_total_stop_import_energy(get_total_stop_import_energy_cmd);

        std::vector<uint8_t> get_total_start_export_energy_cmd{};
        app_layer.create_command_get_total_start_export_energy(get_total_start_export_energy_cmd);

        std::vector<uint8_t> get_total_stop_export_energy_cmd{};
        app_layer.create_command_get_total_stop_export_energy(get_total_stop_export_energy_cmd);

        // std::vector<uint8_t> get_total_transaction_duration_cmd{};
        // app_layer.create_command_get_total_transaction_duration(get_total_transaction_duration_cmd);

        std::vector<uint8_t> get_ocmf_stats_cmd{};
        app_layer.create_command_get_ocmf_stats(get_ocmf_stats_cmd);

        std::vector<uint8_t> get_last_transaction_ocmf_cmd{};
        app_layer.create_command_get_last_transaction_ocmf(get_last_transaction_ocmf_cmd);

        std::vector<uint8_t> get_ocmf_info_cmd{};
        app_layer.create_command_get_ocmf_info(get_ocmf_info_cmd);

        std::vector<uint8_t> get_ocmf_config_cmd{};
        app_layer.create_command_get_ocmf_config(get_ocmf_config_cmd);

        std::vector<uint8_t> slip_msg_read_device_data = std::move(this->slip.package_multi(this->config.powermeter_device_id,
                                                                                            {
                                                                                                get_time_cmd,
                                                                                                get_total_start_import_energy_cmd,
                                                                                                get_total_stop_import_energy_cmd,
                                                                                                get_total_start_export_energy_cmd,
                                                                                                get_total_stop_export_energy_cmd,
                                                                                                // get_total_transaction_duration_cmd,
                                                                                                get_ocmf_stats_cmd,
                                                                                                get_last_transaction_ocmf_cmd,
                                                                                                get_ocmf_info_cmd,
                                                                                                get_ocmf_config_cmd
                                                                                            }));

        this->serial_device.tx(slip_msg_read_device_data);
        receive_response();
    }
    {
        std::vector<uint8_t> get_total_dev_import_energy_cmd{};
        app_layer.create_command_get_total_dev_import_energy(get_total_dev_import_energy_cmd);

        std::vector<uint8_t> get_total_dev_export_energy_cmd{};
        app_layer.create_command_get_total_dev_export_energy(get_total_dev_export_energy_cmd);

        std::vector<uint8_t> slip_msg_read_device_data_2 = std::move(this->slip.package_multi(this->config.powermeter_device_id,
                                                                                              {
                                                                                                  get_total_dev_import_energy_cmd,
                                                                                                  get_total_dev_export_energy_cmd
                                                                                              }));

        this->serial_device.tx(slip_msg_read_device_data_2);
        receive_response();
    }
}

void powermeterImpl::get_device_public_key() {
    {
        std::vector<uint8_t> get_device_public_key_cmd{};
        // TODO (LAD): check which is best / applies to use case
        // create_command_get_pubkey_str16()
        // create_command_get_pubkey_asn1()
        // create_command_get_meter_pubkey()
        app_layer.create_command_get_pubkey_str16(get_device_public_key_cmd);

        std::vector<uint8_t> slip_msg_get_device_public_key = std::move(this->slip.package_single(this->config.powermeter_device_id, get_device_public_key_cmd));

        this->serial_device.tx(slip_msg_get_device_public_key);
        receive_response();
    }
}

void powermeterImpl::request_device_type() {
    std::vector<uint8_t> data_vect{};
    app_layer.create_command_get_device_type(data_vect);
    std::vector<uint8_t> slip_msg_device_type = std::move(this->slip.package_single(this->config.powermeter_device_id, data_vect));

    this->serial_device.tx(slip_msg_device_type);
    receive_response();
}

/* retrieve last error objects from device */
void powermeterImpl::error_diagnostics(uint8_t addr) {
    std::vector<uint8_t> last_log_entry_cmd{};
    app_layer.create_command_get_last_log_entry(last_log_entry_cmd);

    std::vector<uint8_t> last_system_errors_cmd{};
    app_layer.create_command_get_errors(ast_app_layer::ErrorCategory::LAST_ERRORS,
                                        ast_app_layer::ErrorSource::SYSTEM_ERRORS,
                                        last_system_errors_cmd);

    std::vector<uint8_t> last_critical_system_errors_cmd{};
    app_layer.create_command_get_errors(ast_app_layer::ErrorCategory::LAST_CRITICAL_ERRORS,
                                        ast_app_layer::ErrorSource::SYSTEM_ERRORS,
                                        last_critical_system_errors_cmd);

    std::vector<uint8_t> last_comm_errors_cmd{};
    app_layer.create_command_get_errors(ast_app_layer::ErrorCategory::LAST_ERRORS,
                                        ast_app_layer::ErrorSource::COMMUNICATION_ERRORS,
                                        last_comm_errors_cmd);

    std::vector<uint8_t> last_critical_comm_errors_cmd{};
    app_layer.create_command_get_errors(ast_app_layer::ErrorCategory::LAST_CRITICAL_ERRORS,
                                        ast_app_layer::ErrorSource::COMMUNICATION_ERRORS,
                                        last_critical_comm_errors_cmd);

    std::vector<uint8_t> slip_msg_error_diag = std::move(this->slip.package_multi(this->config.powermeter_device_id,
                                                                                  {
                                                                                      last_log_entry_cmd,
                                                                                      last_system_errors_cmd,
                                                                                      last_critical_system_errors_cmd,
                                                                                      last_comm_errors_cmd,
                                                                                      last_critical_comm_errors_cmd
                                                                                  }));

    this->serial_device.tx(slip_msg_error_diag);
    receive_response();
}

void powermeterImpl::read_diagnostics_data() {

    // part 1 - basic info
    {
        std::vector<uint8_t> get_charge_point_id_cmd{};
        app_layer.create_command_get_charge_point_id(get_charge_point_id_cmd);

        std::vector<uint8_t> get_device_type_cmd{};
        app_layer.create_command_get_device_type(get_device_type_cmd);
        
        std::vector<uint8_t> get_hardware_version_cmd{};
        app_layer.create_command_get_hardware_version(get_hardware_version_cmd);

        std::vector<uint8_t> get_application_board_server_id_cmd{};
        app_layer.create_command_get_application_board_server_id(get_application_board_server_id_cmd);

        std::vector<uint8_t> get_application_board_mode_cmd{};
        app_layer.create_command_get_application_board_mode(get_application_board_mode_cmd);

        std::vector<uint8_t> slip_msg_get_diagnostics_data_1 = std::move(this->slip.package_multi(this->config.powermeter_device_id,
                                                                                                  {
                                                                                                      get_charge_point_id_cmd,
                                                                                                      get_device_type_cmd,
                                                                                                      get_hardware_version_cmd,
                                                                                                      get_application_board_server_id_cmd,
                                                                                                      get_application_board_mode_cmd
                                                                                                  }));

        this->serial_device.tx(slip_msg_get_diagnostics_data_1);
        receive_response();
    }

    // part 2 - log stats
    {
        std::vector<uint8_t> get_log_stats_cmd{};
        app_layer.create_command_get_log_stats(get_log_stats_cmd);

        std::vector<uint8_t> slip_msg_get_diagnostics_data_2 = std::move(this->slip.package_single(this->config.powermeter_device_id, get_log_stats_cmd));

        this->serial_device.tx(slip_msg_get_diagnostics_data_2);
        receive_response();
    }

    // part 3 - HW/SW info
    {
        std::vector<uint8_t> get_application_board_serial_number_cmd{};
        app_layer.create_command_get_application_board_serial_number(get_application_board_serial_number_cmd);

        std::vector<uint8_t> get_application_board_software_version_cmd{};
        app_layer.create_command_get_application_board_software_version(get_application_board_software_version_cmd);
        
        std::vector<uint8_t> get_application_board_fw_checksum_cmd{};
        app_layer.create_command_get_application_board_fw_checksum(get_application_board_fw_checksum_cmd);

        std::vector<uint8_t> get_application_board_fw_hash_cmd{};
        app_layer.create_command_get_application_board_fw_hash(get_application_board_fw_hash_cmd);

        std::vector<uint8_t> get_metering_board_software_version_cmd{};
        app_layer.create_command_get_metering_board_software_version(get_metering_board_software_version_cmd);

        std::vector<uint8_t> get_metering_board_fw_checksum_cmd{};
        app_layer.create_command_get_metering_board_fw_checksum(get_metering_board_fw_checksum_cmd);
        

        std::vector<uint8_t> slip_msg_get_diagnostics_data_3 = std::move(this->slip.package_multi(this->config.powermeter_device_id,
                                                                                                  {
                                                                                                      get_application_board_serial_number_cmd,
                                                                                                      get_application_board_software_version_cmd,
                                                                                                      get_application_board_fw_checksum_cmd,
                                                                                                      get_application_board_fw_hash_cmd,
                                                                                                      get_metering_board_software_version_cmd,
                                                                                                      get_metering_board_fw_checksum_cmd
                                                                                                  }));

        this->serial_device.tx(slip_msg_get_diagnostics_data_3);
        receive_response();
    }
}

void powermeterImpl::read_powermeter_values() {
    this->readRegisters();
    this->pm_last_values.timestamp = Everest::Date::to_rfc3339(date::utc_clock::now());
}

void powermeterImpl::init_default_values() {
    this->pm_last_values.timestamp = Everest::Date::to_rfc3339(date::utc_clock::now());
    this->pm_last_values.meter_id = "AST_Powermeter_addr_" + std::to_string(this->config.powermeter_device_id);

    this->pm_last_values.energy_Wh_import.total = 0.0f;
    // this->pm_last_values.energy_Wh_import.L1 = 0.0f;

    types::units::Energy energy_Wh;
    energy_Wh.total = 0.0f;
    this->pm_last_values.energy_Wh_export = energy_Wh;
    // this->pm_last_values.energy_Wh_export.L1 = 0.0f;

    types::units::Power power_W;
    power_W.total = 0.0f;
    this->pm_last_values.power_W = power_W;
    // this->pm_last_values.power_W.L1 = 0.0f;

    types::units::Voltage voltage_V;
    voltage_V.DC = 0.0f;
    this->pm_last_values.voltage_V = voltage_V;
    // this->pm_last_values.voltage_V.L1 = 0.0f;

    types::units::Current current_A;
    current_A.DC = 0.0f;
    this->pm_last_values.current_A = current_A;
}

void powermeterImpl::readRegisters() {
    std::vector<uint8_t> get_voltage_cmd{};
    app_layer.create_command_get_voltage(get_voltage_cmd);

    std::vector<uint8_t> get_current_cmd{};
    app_layer.create_command_get_current(get_current_cmd);

    std::vector<uint8_t> get_import_power_cmd{};
    app_layer.create_command_get_import_power(get_import_power_cmd);

    std::vector<uint8_t> export_power_cmd{};
    app_layer.create_command_get_export_power(export_power_cmd);

    std::vector<uint8_t> get_total_power_cmd{};
    app_layer.create_command_get_total_power(get_total_power_cmd);

    std::vector<uint8_t> slip_msg_read_registers = std::move(this->slip.package_multi(this->config.powermeter_device_id,
                                                                                      {
                                                                                          get_voltage_cmd,
                                                                                          get_current_cmd,
                                                                                          get_import_power_cmd,
                                                                                          export_power_cmd,
                                                                                          get_total_power_cmd
                                                                                      }));

    this->serial_device.tx(slip_msg_read_registers);
    receive_response();
}


// ############################################################################################################################################
// ############################################################################################################################################

void powermeterImpl::process_response(const std::vector<uint8_t>& response_message) {
    size_t response_size = response_message.size();

    if (response_size == slip_protocol::SLIP_SIZE_ON_ERROR) {
        // error handling
        if (response_message == slip_protocol::SLIP_ERROR_SIZE_ERROR) {
            EVLOG_error << "Response broken: Size of message is too short!";
        } else if (response_message == slip_protocol::SLIP_ERROR_MALFORMED) {
            EVLOG_error << "Response broken: Malformed message received!";
        } else if (response_message == slip_protocol::SLIP_ERROR_CRC_MISMATCH) {
            EVLOG_error << "Response broken: CRC mismatch!";
        } else {
            EVLOG_error << "Response broken: Unknown error";
        }
    } else {
        // split into multiple command responses
        uint8_t dest_addr = response_message.at(0);
        uint16_t i = 1;
        while ((i + 4) <= response_size) {
            uint16_t part_cmd = ((uint16_t)response_message.at(i + 1) << 8) | response_message.at(i);
            uint16_t part_len = ((uint16_t)response_message.at(i + 3) << 8) | response_message.at(i + 2);
            ast_app_layer::CommandResult part_status = static_cast<ast_app_layer::CommandResult>(response_message.at(i + 4));

            if ((i + part_len - 1) <= response_size) {
                std::vector<uint8_t> part_data((response_message.begin() + i + 5), (response_message.begin() + i + part_len));

                EVLOG_error << "\n\n"
                            << "response received from ID " << int(dest_addr) << ": \n"
                            << "    cmd: 0x" << hexdump_u16(part_cmd) 
                            << "   len: " << part_len 
                            << "   status: " << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint8_t>(part_status)
                            << "   data: " << ((part_len > 5) ? hexdump(part_data) : "none") 
                            << "\n\n";

                if (part_status != ast_app_layer::CommandResult::OK) {
                    EVLOG_error << "Powermeter has signaled an error (\"" 
                                << ast_app_layer::command_result_to_string(part_status) 
                                << "\")! Retrieving diagnostics data...";
                    error_diagnostics(dest_addr);
                }

                // process response
                switch (part_cmd) {

                // operational values

                    case (int)ast_app_layer::CommandType::START_TRANSACTION:
                        {
                            EVLOG_info << "(START_TRANSACTION) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::STOP_TRANSACTION:
                        {
                            EVLOG_info << "(STOP_TRANSACTION) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::TIME:
                        {
                            EVLOG_info << "(TIME) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_VOLTAGE_L1:
                        {
                            types::units::Voltage volt = this->pm_last_values.voltage_V.value();
                            volt.DC = (float)(((uint32_t)part_data[3] << 24) | 
                                              ((uint32_t)part_data[2] << 16) | 
                                              ((uint32_t)part_data[1] <<  8) | 
                                               (uint32_t)part_data[0]);
                            this->pm_last_values.voltage_V = volt;
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_CURRENT_L1:
                        {
                            types::units::Current amp = this->pm_last_values.current_A.value();
                            amp.DC = (float)(((uint32_t)part_data[3] << 24) | 
                                             ((uint32_t)part_data[2] << 16) | 
                                             ((uint32_t)part_data[1] <<  8) | 
                                              (uint32_t)part_data[0]) / 1000.0;  // powermeter reports in [mA]
                            this->pm_last_values.current_A = amp;
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_IMPORT_DEV_POWER:
                        {
                            types::units::Power power = this->pm_last_values.power_W.value();
                            power.total = (float)(((uint32_t)part_data[3] << 24) | 
                                                  ((uint32_t)part_data[2] << 16) | 
                                                  ((uint32_t)part_data[1] <<  8) | 
                                                   (uint32_t)part_data[0]) / 100.0;  // powermeter reports in [W * 100]
                            this->pm_last_values.power_W = power;
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_EXPORT_DEV_POWER:
                        {
                            EVLOG_info << "(GET_EXPORT_DEV_POWER) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_DEV_POWER:
                        {
                            EVLOG_info << "(GET_TOTAL_DEV_POWER) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_IMPORT_DEV_ENERGY:
                        {
                            types::units::Energy energy_in = this->pm_last_values.energy_Wh_import;
                            energy_in.total = (float)(((uint64_t)part_data[7] << 56) |
                                                      ((uint64_t)part_data[6] << 48) |
                                                      ((uint64_t)part_data[5] << 40) |
                                                      ((uint64_t)part_data[4] << 32) |
                                                      ((uint64_t)part_data[3] << 24) |
                                                      ((uint64_t)part_data[2] << 16) |
                                                      ((uint64_t)part_data[1] <<  8) |
                                                       (uint64_t)part_data[0]) / 10.0;  // powermeter reports in [Wh * 10]
                            this->pm_last_values.energy_Wh_import = energy_in;
                            EVLOG_info << "(GET_TOTAL_IMPORT_DEV_ENERGY) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_EXPORT_DEV_ENERGY:
                        {
                            types::units::Energy energy_out{};
                            if (this->pm_last_values.energy_Wh_export.has_value()) {
                                energy_out = this->pm_last_values.energy_Wh_export.value();
                            }
                            energy_out.total = (float)(((uint64_t)part_data[7] << 56) |
                                                       ((uint64_t)part_data[6] << 48) |
                                                       ((uint64_t)part_data[5] << 40) |
                                                       ((uint64_t)part_data[4] << 32) |
                                                       ((uint64_t)part_data[3] << 24) |
                                                       ((uint64_t)part_data[2] << 16) |
                                                       ((uint64_t)part_data[1] <<  8) |
                                                        (uint64_t)part_data[0]) / 10.0;  // powermeter reports in [Wh * 10]
                            this->pm_last_values.energy_Wh_export = energy_out;
                            EVLOG_info << "(GET_TOTAL_EXPORT_DEV_ENERGY) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_START_IMPORT_DEV_ENERGY:
                        {
                            EVLOG_info << "(GET_TOTAL_START_IMPORT_DEV_ENERGY) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_STOP_IMPORT_DEV_ENERGY:
                        {
                            EVLOG_info << "(GET_TOTAL_STOP_IMPORT_DEV_ENERGY) Not yet implemented.";   
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_START_EXPORT_DEV_ENERGY:
                        {
                            EVLOG_info << "(GET_TOTAL_START_EXPORT_DEV_ENERGY) Not yet implemented.";   
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TOTAL_STOP_EXPORT_DEV_ENERGY:
                        {
                            EVLOG_info << "(GET_TOTAL_STOP_EXPORT_DEV_ENERGY) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_TRANSACT_TOTAL_DURATION:
                        {
                            EVLOG_info << "(GET_TRANSACT_TOTAL_DURATION) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_PUBKEY_STR16:
                        {
                            EVLOG_info << "(GET_PUBKEY_STR16) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_PUBKEY_ASN1:
                        {
                            EVLOG_info << "(GET_PUBKEY_ASN1) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::REQUEST_METER_PUBKEY:
                        {
                            EVLOG_info << "(REQUEST_METER_PUBKEY) Not yet implemented.";
                        }
                        break;

                // diagnostics

                    case (int)ast_app_layer::CommandType::OCMF_STATS:
                        {
                            EVLOG_info << "(OCMF_STATS) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_OCMF:
                        {
                            EVLOG_info << "(GET_OCMF) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_LAST_OCMF:
                        {
                            EVLOG_info << "(GET_LAST_OCMF) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::OCMF_INFO:
                        {
                            EVLOG_info << "(OCMF_INFO) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::OCMF_CONFIG:
                        {
                            EVLOG_info << "(OCMF_CONFIG) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::CHARGE_POINT_ID:
                        {
                            EVLOG_info << "(CHARGE_POINT_ID) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_ERRORS:
                        {
                            EVLOG_info << "(GET_ERRORS) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_LOG_STATS:
                        {
                            EVLOG_info << "(GET_LOG_STATS) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_LOG_ENTRY:
                        {
                            EVLOG_info << "(GET_LOG_ENTRY) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_LAST_LOG_ENTRY:
                        {
                            EVLOG_info << "(GET_LAST_LOG_ENTRY) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::GET_LOG_ENTRY_REVERSE:
                        {
                            EVLOG_info << "(GET_LOG_ENTRY_REVERSE) Not yet implemented.";
                        }
                        break;

                // device parameters

                    case (int)ast_app_layer::CommandType::AB_MODE_SET:
                        {
                            EVLOG_info << "(AB_MODE_SET) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_HW_VERSION:
                        {
                            EVLOG_info << "(AB_HW_VERSION) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_SERVER_ID:
                        {
                            EVLOG_info << "(AB_SERVER_ID) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_SERIAL_NR:
                        {
                            EVLOG_info << "(AB_SERIAL_NR) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_SW_VERSION:
                        {
                            EVLOG_info << "(AB_SW_VERSION) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_FW_CHECKSUM:
                        {
                            EVLOG_info << "(AB_FW_CHECKSUM) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_FW_HASH:
                        {
                            EVLOG_info << "(AB_FW_HASH) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::MB_SW_VERSION:
                        {
                            EVLOG_info << "(MB_SW_VERSION) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::MB_FW_CHECKSUM:
                        {
                            EVLOG_info << "(MB_FW_CHECKSUM) Not yet implemented.";
                        }
                        break;

                    case (int)ast_app_layer::CommandType::AB_DEVICE_TYPE:
                        {
                            std::string device_name{};
                            for (auto letter : part_data) {
                                device_name += letter;
                            }
                            EVLOG_error << "received AB_DEVICE_TYPE: " << device_name;
                        }
                        break;

                // not (yet) implemented

                    case (int)ast_app_layer::CommandType::RESET_DC_METER:
                    case (int)ast_app_layer::CommandType::MEASUREMENT_MODE:
                    case (int)ast_app_layer::CommandType::GET_NORMAL_VOLTAGE:
                    case (int)ast_app_layer::CommandType::GET_NORMAL_CURRENT:
                    case (int)ast_app_layer::CommandType::GET_MAX_CURRENT:
                    case (int)ast_app_layer::CommandType::LINE_LOSS_IMPEDANCE:
                    case (int)ast_app_layer::CommandType::LINE_LOSS_MEAS_MODE:
                    case (int)ast_app_layer::CommandType::TEMPERATURE:
                    case (int)ast_app_layer::CommandType::AB_STATUS:
                    case (int)ast_app_layer::CommandType::METER_BUS_ADDR:
                    case (int)ast_app_layer::CommandType::GET_OCMF_REVERSE:
                    case (int)ast_app_layer::CommandType::GET_PUBKEY_BIN:
                    case (int)ast_app_layer::CommandType::GET_TRANSACT_IMPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TRANSACT_EXPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TRANSACT_TOTAL_IMPORT_DEV_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TRANSACT_TOTAL_EXPORT_DEV_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TRANSACT_TOTAL_IMPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TRANSACT_TOTAL_EXPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_START_IMPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_START_EXPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_START_IMPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_START_EXPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_STOP_IMPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_STOP_EXPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_STOP_IMPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_STOP_EXPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::REGISTER_DISPLAY_PUBKEY:
                    case (int)ast_app_layer::CommandType::REQUEST_CHALLENGE:
                    case (int)ast_app_layer::CommandType::SET_SIGNED_CHALLENGE:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_IMPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_EXPORT_MAINS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_IMPORT_MAINS_POWER:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_EXPORT_MAINS_POWER:
                    case (int)ast_app_layer::CommandType::GET_DEV_VOLTAGE_L1:
                    case (int)ast_app_layer::CommandType::GET_IMPORT_LINE_LOSS_POWER:
                    case (int)ast_app_layer::CommandType::GET_EXPORT_LINE_LOSS_POWER:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_IMPORT_LINE_LOSS_ENERGY:
                    case (int)ast_app_layer::CommandType::GET_TOTAL_EXPORT_LINE_LOSS_ENERGY:

                    case (int)ast_app_layer::CommandType::GET_SECOND_INDEX:
                    case (int)ast_app_layer::CommandType::GET_PUBKEY_STR32:
                    case (int)ast_app_layer::CommandType::GET_PUBKEY_CSTR16:
                    case (int)ast_app_layer::CommandType::GET_PUBKEY_CSTR32:
                    case (int)ast_app_layer::CommandType::REPEAT_DATA:
                    case (int)ast_app_layer::CommandType::AB_DMC:
                    case (int)ast_app_layer::CommandType::AB_PROD_DATE:
                    case (int)ast_app_layer::CommandType::SET_REQUEST_CHALLENGE:
                        {
                            EVLOG_error << "Command not (yet) implemented.";
                        }
                        break;

                    default:
                        {
                            EVLOG_error << "Command ID invalid.";
                        }
                        break;
                }

            } else {
                EVLOG_error << "Error: Malformed data.";
            }
            i += part_len;
        }

        // publish powermeter values
        this->publish_powermeter(this->pm_last_values);
    }
}

void powermeterImpl::receive_response() {
    std::vector<uint8_t> response{};
    response.reserve(ast_app_layer::PM_AST_MAX_RX_LENGTH);
    this->serial_device.rx(response, ast_app_layer::PM_AST_SERIAL_RX_INITIAL_TIMEOUT_MS, ast_app_layer::PM_AST_SERIAL_RX_WITHIN_MESSAGE_TIMEOUT_MS);

    // EVLOG_critical << "\n\nRECEIVE: " << hexdump(response) << " length: " << response.size() << "\n\n";

    process_response(std::move(this->slip.unpack(response)));
}

// ############################################################################################################################################
// ############################################################################################################################################
// ############################################################################################################################################

int powermeterImpl::handle_start_transaction(types::powermeter::TransactionParameters& transaction_parameters) {
                                                // (bool transaction_assigned_to_user,
                                            //   ast_app_layer::UserIdType user_id_type,
                                            //   std::string user_id_data) {
    ast_app_layer::UserIdStatus user_id_status = ast_app_layer::UserIdStatus::USER_NOT_ASSIGNED;
    if (transaction_parameters.transaction_assigned_to_user) {
        user_id_status = ast_app_layer::UserIdStatus::USER_ASSIGNED;
    }

    
    // std::vector<uint8_t> data_vect{};
    // app_layer.create_command_start_transaction(user_id_status, user_id_type, user_id_data, data_vect);
    // std::vector<uint8_t> slip_msg_start_transaction = std::move(this->slip.package_single(this->config.powermeter_device_id, data_vect));

    // this->serial_device.tx(slip_msg_start_transaction);
    receive_response();

    // return status of device reaction
}

int powermeterImpl::handle_stop_transaction() {
    std::vector<uint8_t> data_vect{};
    app_layer.create_command_stop_transaction(data_vect);
    std::vector<uint8_t> slip_msg_stop_transaction = std::move(this->slip.package_single(this->config.powermeter_device_id, data_vect));

    this->serial_device.tx(slip_msg_stop_transaction);
    receive_response();

    // return status of device reaction
}

std::string powermeterImpl::handle_get_signed_meter_value(std::string& auth_token) {
    // your code for cmd get_signed_meter_value goes here

    //

    return "everest";
}

} // namespace main
} // namespace module
