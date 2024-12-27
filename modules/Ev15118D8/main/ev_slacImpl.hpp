// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef MAIN_EV_SLAC_IMPL_HPP
#define MAIN_EV_SLAC_IMPL_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 3
//

#include <generated/interfaces/ev_slac/Implementation.hpp>

#include "../Ev15118D8.hpp"

// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1
// insert your custom include headers here
#include "wpa_ctrl.h" //lives in /usr/include after installing libwpa-client-dev (sudo apt install libwpa-client-dev)
// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1

namespace module {
namespace main {

struct Conf {
    std::string device;
    int set_key_timeout_ms;
    bool ETT_AC;
    bool ETT_DC;
    bool ETT_WPT;
    bool ETT_ACD;
    std::string VSE_ADDITIONAL_INFORMATION;
    std::string WPA_TYPE;
    std::string certificate_path;
    std::string logging_path;
    std::string private_key_password;
    bool enable_wpa_logging;
    std::string wpa_psk_passphrase;
    std::string wpa_ssid_override;
    std::string eap_tls_identity;
    std::string VSECountryCodeMatch;
    std::string VSEOperatorIDMatch;
    int VSEChargingSiteIDMatch;
    std::string VSEAdditionalInformationMatch;

};

class ev_slacImpl : public ev_slacImplBase {
public:
    ev_slacImpl() = delete;
    ev_slacImpl(Everest::ModuleAdapter* ev, const Everest::PtrContainer<Ev15118D8>& mod, Conf& config) :
        ev_slacImplBase(ev, "main"), mod(mod), config(config){};

    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1
    // insert your public definitions here
    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1

protected:
    // command handler functions (virtual)
    virtual void handle_reset() override;
    virtual bool handle_trigger_matching() override;

    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1
    void run();
    void _handle_trigger_matching();
    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1

private:
    const Everest::PtrContainer<Ev15118D8>& mod;
    const Conf& config;

    virtual void init() override;
    virtual void ready() override;

    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
    //helper functions
     std::string UTF8ToString(std::string CodedString);
     std::string wpa_ctrl_request2(struct wpa_ctrl *ctrl, std::string cmd);
    //The complete EV side VSE in bytes
    std::string EvVSE;
    struct wpa_ctrl *ctrl_conn;
    unsigned char ETT = 0;
    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
};

// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1
// insert other definitions here
// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1

} // namespace main
} // namespace module

#endif // MAIN_EV_SLAC_IMPL_HPP
