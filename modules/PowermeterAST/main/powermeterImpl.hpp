// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef MAIN_POWERMETER_IMPL_HPP
#define MAIN_POWERMETER_IMPL_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 3
//

#include <generated/interfaces/powermeter/Implementation.hpp>

#include "../PowermeterAST.hpp"

// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1
// insert your custom include headers here
#include "ast_app_layer.hpp"
#include "serial_device.hpp"
#include "slip_protocol.hpp"
// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1

namespace module {
namespace main {

struct Conf {
    int powermeter_device_id;
    std::string serial_port;
    int baudrate;
    int parity;
    int rs485_direction_gpio;
    bool ignore_echo;
    bool publish_device_data;
    bool publish_device_diagnostics;
};

class powermeterImpl : public powermeterImplBase {
public:
    powermeterImpl() = delete;
    powermeterImpl(Everest::ModuleAdapter* ev, const Everest::PtrContainer<PowermeterAST>& mod, Conf& config) :
        powermeterImplBase(ev, "main"), mod(mod), config(config){};

    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1
    // insert your public definitions here
    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1

protected:
    // command handler functions (virtual)
    virtual std::string handle_get_signed_meter_value(std::string& auth_token) override;

    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1
    // insert your protected definitions here
    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1

private:
    const Everest::PtrContainer<PowermeterAST>& mod;
    const Conf& config;

    virtual void init() override;
    virtual void ready() override;

    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
    // insert your private definitions here
    class DeviceData {
        public:
            std::string data{};

            json to_json() {
                json j = json::object();
                j["data"] = data;
                return j;
            }

            std::string to_str() {
                json j = json::object();
                j["data"] = data;
                return j.dump();
            }

    };

    class DeviceDiagnostics {
        public:
            std::string data{};

            json to_json() {
                json j = json::object();
                j["data"] = data;
                return j;
            }

            std::string to_str() {
                json j = json::object();
                j["data"] = data;
                return j.dump();
            }
    };

    serial_device::SerialDevice serial_device{};
    slip_protocol::SlipProtocol slip{};
    ast_app_layer::AstAppLayer app_layer{};

    types::powermeter::Powermeter pm_last_values;

    DeviceData device_data_obj{};
    DeviceDiagnostics device_diagnostics_obj{};

    void init_default_values();
    void read_powermeter_values();
    void set_device_time();
    void set_device_charge_point_id(ast_app_layer::UserIdType id_type, std::string charge_point_id);
    void read_device_data();
    void read_diagnostics_data();
    void publish_device_data_topic();
    void publish_device_diagnostics_topic();
    void get_device_public_key();
    void readRegisters();
    void process_response(const std::vector<uint8_t>& register_message);
    void request_device_type();
    void error_diagnostics(uint8_t addr);
    void receive_response();
    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
};

// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1
// insert other definitions here
// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1

} // namespace main
} // namespace module

#endif // MAIN_POWERMETER_IMPL_HPP
