// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

//RDB To allow both ISO-2 and ISO-20, we need to switch between them after SAP is handled.
//Create both controllers on init, and then do the switch after SAP is handled.
//Also, create a new controller just for SAP.
//Brute force method, probably there is a more elegant way.

#include "ISO15118_chargerImpl.hpp"

#include "session_logger.hpp"

#include <iso15118/io/logging.hpp>
#include <iso15118/session/feedback.hpp>
#include <iso15118/session/logger.hpp>
#include <iso15118/session_d2/feedback.hpp>
#include <iso15118/session_d2/logger.hpp>
#include <iso15118/session_d2_sap/feedback.hpp>
#include <iso15118/session_d2_sap/logger.hpp>
#include <iso15118/tbd_controller.hpp>
#include <iso15118/tbd_controller_d2.hpp>
#include <iso15118/tbd_controller_d2_sap.hpp>

std::unique_ptr<iso15118::TbdController> controller_20;
std::unique_ptr<iso15118::TbdController_2> controller_2;
std::unique_ptr<iso15118::TbdController_2_sap> controller_2_sap;

iso15118::TbdConfig tbd_config_20;
iso15118::TbdConfig_2 tbd_config_2;
iso15118::TbdConfig_2_sap tbd_config_2_sap;

iso15118::session::feedback::Callbacks callbacks_20;
iso15118::session_2::feedback::Callbacks callbacks_2;
iso15118::session_2_sap::feedback::Callbacks callbacks_2_sap;

std::unique_ptr<SessionLogger> session_logger;


namespace module {
namespace charger {

static std::filesystem::path get_cert_path(const std::filesystem::path& initial_path, const std::string& config_path) {
    if (config_path.empty()) {
        return initial_path;
    }

    if (*config_path.begin() == '/') {
        return config_path;
    } else {
        return initial_path / config_path;
    }
}

static iso15118::config::TlsNegotiationStrategy convert_tls_negotiation_strategy(const std::string& strategy) {
    using Strategy = iso15118::config::TlsNegotiationStrategy;
    if (strategy.compare("ACCEPT_CLIENT_OFFER") == 0) {
        return Strategy::ACCEPT_CLIENT_OFFER;
    } else if (strategy.compare("ENFORCE_TLS") == 0) {
        return Strategy::ENFORCE_TLS;
    } else if (strategy.compare("ENFORCE_NO_TLS") == 0) {
        return Strategy::ENFORCE_NO_TLS;
    } else {
        EVLOG_AND_THROW(Everest::EverestConfigError("Invalid choice for tls_negotiation_strategy: " + strategy));
        // better safe than sorry
    }
}

void ISO15118_chargerImpl::init() {
    // setup logging routine
    iso15118::io::set_logging_callback([](const std::string& msg) { EVLOG_info << msg; });

    session_logger = std::make_unique<SessionLogger>(mod->config.logging_path);

    callbacks_20.dc_charge_target = [this](const iso15118::session::feedback::DcChargeTarget& charge_target) {
        types::iso15118_charger::DC_EVTargetValues tmp;
        tmp.DC_EVTargetVoltage = charge_target.voltage;
        tmp.DC_EVTargetCurrent = charge_target.current;
        publish_DC_EVTargetVoltageCurrent(tmp);
    };

    callbacks_2.dc_charge_target = [this](const iso15118::session_2::feedback::DcChargeTarget& charge_target) {
        types::iso15118_charger::DC_EVTargetValues tmp;
        tmp.DC_EVTargetVoltage = charge_target.voltage;
        tmp.DC_EVTargetCurrent = charge_target.current;
        publish_DC_EVTargetVoltageCurrent(tmp);
    };

    callbacks_2_sap.dc_charge_target = [this](const iso15118::session_2_sap::feedback::DcChargeTarget& charge_target) {
        types::iso15118_charger::DC_EVTargetValues tmp;
        tmp.DC_EVTargetVoltage = charge_target.voltage;
        tmp.DC_EVTargetCurrent = charge_target.current;
        publish_DC_EVTargetVoltageCurrent(tmp);
    };

    callbacks_20.dc_max_limits = [this](const iso15118::session::feedback::DcMaximumLimits& max_limits) {
        types::iso15118_charger::DC_EVMaximumLimits tmp;
        tmp.DC_EVMaximumVoltageLimit = max_limits.voltage;
        tmp.DC_EVMaximumCurrentLimit = max_limits.current;
        tmp.DC_EVMaximumPowerLimit = max_limits.power;
        publish_DC_EVMaximumLimits(tmp);
    };

    callbacks_2.dc_max_limits = [this](const iso15118::session_2::feedback::DcMaximumLimits& max_limits) {
        types::iso15118_charger::DC_EVMaximumLimits tmp;
        tmp.DC_EVMaximumVoltageLimit = max_limits.voltage;
        tmp.DC_EVMaximumCurrentLimit = max_limits.current;
        tmp.DC_EVMaximumPowerLimit = max_limits.power;
        publish_DC_EVMaximumLimits(tmp);
    };

    callbacks_2_sap.dc_max_limits = [this](const iso15118::session_2_sap::feedback::DcMaximumLimits& max_limits) {
        types::iso15118_charger::DC_EVMaximumLimits tmp;
        tmp.DC_EVMaximumVoltageLimit = max_limits.voltage;
        tmp.DC_EVMaximumCurrentLimit = max_limits.current;
        tmp.DC_EVMaximumPowerLimit = max_limits.power;
        publish_DC_EVMaximumLimits(tmp);
    };

    callbacks_20.signal = [this](iso15118::session::feedback::Signal signal) {
        using Signal = iso15118::session::feedback::Signal;
        switch (signal) {
        case Signal::CHARGE_LOOP_STARTED:
            publish_currentDemand_Started(nullptr);
            break;
        case Signal::SETUP_FINISHED:
            publish_V2G_Setup_Finished(nullptr);
            break;
            ;
        case Signal::START_CABLE_CHECK:
            publish_Start_CableCheck(nullptr);
            break;
        case Signal::REQUIRE_AUTH_EIM:
            publish_Require_Auth_EIM(nullptr);
            break;
        case Signal::CHARGE_LOOP_FINISHED:
            publish_currentDemand_Finished(nullptr);
            break;
        case Signal::DC_OPEN_CONTACTOR:
            publish_DC_Open_Contactor(nullptr);
            break;
        case Signal::DLINK_TERMINATE:
            publish_dlink_terminate(nullptr);
            break;
        case Signal::DLINK_PAUSE:
            publish_dlink_pause(nullptr);
            break;
        case Signal::DLINK_ERROR:
            publish_dlink_error(nullptr);
            break;
        }
    };

    callbacks_2.signal = [this](iso15118::session_2::feedback::Signal signal) {
        using Signal = iso15118::session_2::feedback::Signal;
        switch (signal) {
        case Signal::CHARGE_LOOP_STARTED:
            publish_currentDemand_Started(nullptr);
            break;
        case Signal::SETUP_FINISHED:
            publish_V2G_Setup_Finished(nullptr);
            break;
            ;
        case Signal::START_CABLE_CHECK:
            publish_Start_CableCheck(nullptr);
            break;
        case Signal::REQUIRE_AUTH_EIM:
            publish_Require_Auth_EIM(nullptr);
            break;
        case Signal::CHARGE_LOOP_FINISHED:
            publish_currentDemand_Finished(nullptr);
            break;
        case Signal::DC_OPEN_CONTACTOR:
            publish_DC_Open_Contactor(nullptr);
            break;
        case Signal::DLINK_TERMINATE:
            publish_dlink_terminate(nullptr);
            break;
        case Signal::DLINK_PAUSE:
            publish_dlink_pause(nullptr);
            break;
        case Signal::DLINK_ERROR:
            publish_dlink_error(nullptr);
            break;
        }
    };


    callbacks_2_sap.signal = [this](iso15118::session_2_sap::feedback::Signal signal) {
        using Signal = iso15118::session_2_sap::feedback::Signal;
        switch (signal) {
        case Signal::CHARGE_LOOP_STARTED:
            publish_currentDemand_Started(nullptr);
            break;
        case Signal::SETUP_FINISHED:
            publish_V2G_Setup_Finished(nullptr);
            break;
            ;
        case Signal::START_CABLE_CHECK:
            publish_Start_CableCheck(nullptr);
            break;
        case Signal::REQUIRE_AUTH_EIM:
            publish_Require_Auth_EIM(nullptr);
            break;
        case Signal::CHARGE_LOOP_FINISHED:
            publish_currentDemand_Finished(nullptr);
            break;
        case Signal::DC_OPEN_CONTACTOR:
            publish_DC_Open_Contactor(nullptr);
            break;
        case Signal::DLINK_TERMINATE:
            publish_dlink_terminate(nullptr);
            break;
        case Signal::DLINK_PAUSE:
            publish_dlink_pause(nullptr);
            break;
        case Signal::DLINK_ERROR:
            publish_dlink_error(nullptr);
            break;
        }
    };

    const auto default_cert_path = mod->info.paths.etc / "certs";

    const auto cert_path = get_cert_path(default_cert_path, mod->config.certificate_path);

    tbd_config_2 = {
        {
            iso15118::config::CertificateBackend::EVEREST_LAYOUT,
            cert_path.string(),
            mod->config.private_key_password,
            mod->config.enable_ssl_logging,
        },
        mod->config.device,
        convert_tls_negotiation_strategy(mod->config.tls_negotiation_strategy),
    };


    tbd_config_2_sap = {
        {
            iso15118::config::CertificateBackend::EVEREST_LAYOUT,
            cert_path.string(),
            mod->config.private_key_password,
            mod->config.enable_ssl_logging,
        },
        mod->config.device,
        convert_tls_negotiation_strategy(mod->config.tls_negotiation_strategy),
    };

    tbd_config_20 = {
        {
            iso15118::config::CertificateBackend::EVEREST_LAYOUT,
            cert_path.string(),
            mod->config.private_key_password,
            mod->config.enable_ssl_logging,
        },
        mod->config.device,
        convert_tls_negotiation_strategy(mod->config.tls_negotiation_strategy),
    };


    //RDB set up all three worlds However, we need to only use a single socket fd and IP connection, so this needs to be passed in later and not initialized
    //in ISO2 and ISO20.
    controller_2_sap = std::make_unique<iso15118::TbdController_2_sap>(tbd_config_2_sap, callbacks_2_sap);
    controller_20 = std::make_unique<iso15118::TbdController>(tbd_config_20, callbacks_20);
    controller_2 = std::make_unique<iso15118::TbdController_2>(tbd_config_2, callbacks_2);
}

void ISO15118_chargerImpl::ready() {

    //RDB - test switching between worlds

    int SAP_Version=-1;

    // FIXME (aw): this is just plain stupid ...
    while (true) {
        try {
            // RDB - switch between the three controllers
            if (SAP_Version == -1) {

                controller_2_sap->loop();
                // RDB At this point, the SDP is handled, and the IConnection is made. Give these to either ISO2 or
                // ISO20 depending on the SAP outcome
                SAP_Version = controller_2_sap->get_SAP_Version();

                // RDB - set up the selected controller to take over
                if (SAP_Version == 20) {
                    controller_20->set_SAP_IConnection(controller_2_sap->get_SAP_IConnection());
                    controller_20->set_PollManager(controller_2_sap->GetPollManager());
                } else if (SAP_Version == 2) {
                    controller_2->set_SAP_IConnection(controller_2_sap->get_SAP_IConnection());
                    controller_2->set_PollManager(controller_2_sap->GetPollManager());
                }
            } else if (SAP_Version == 20) {
                controller_20->loop();
            } else if (SAP_Version == 2) {
                controller_2->loop();
            } else {
                throw("Unsupported SAP version");
            }
        } catch (const std::exception& e) {
            EVLOG_error << "D20Evse crashed: " << e.what();
            publish_dlink_error(nullptr);
        }

        // RDB - tbd, what to do here with the three controllers?
        //   controller_2_sap.reset();

        //  const auto RETRY_INTERVAL = std::chrono::milliseconds(1000);

        //  EVLOG_info << "Trying to restart in " << std::to_string(RETRY_INTERVAL.count()) << " milliseconds";

        //  std::this_thread::sleep_for(RETRY_INTERVAL);

        // SAP_Version=-1;
        //  controller_2_sap = std::make_unique<iso15118::TbdController_2_sap>(tbd_config_2_sap, callbacks_2_sap);
    }
}

void ISO15118_chargerImpl::handle_setup(
    types::iso15118_charger::EVSEID& evse_id,
    std::vector<types::iso15118_charger::EnergyTransferMode>& supported_energy_transfer_modes,
    types::iso15118_charger::SAE_J2847_Bidi_Mode& sae_j2847_mode, bool& debug_mode,
    types::iso15118_charger::SetupPhysicalValues& physical_values) {
    // your code for cmd setup goes here
}

void ISO15118_chargerImpl::handle_session_setup(std::vector<types::iso15118_charger::PaymentOption>& payment_options,
                                                bool& supported_certificate_service) {
    // your code for cmd session_setup goes here
}

void ISO15118_chargerImpl::handle_certificate_response(
    types::iso15118_charger::Response_Exi_Stream_Status& exi_stream_status) {
    // your code for cmd certificate_response goes here
}

void ISO15118_chargerImpl::handle_authorization_response(
    types::authorization::AuthorizationStatus& authorization_status,
    types::authorization::CertificateStatus& certificate_status) {

    // Todo(sl): Currently PnC is not supported
    bool authorized = false;

    if (authorization_status == types::authorization::AuthorizationStatus::Accepted) {
        authorized = true;
    }

    //RDB Send to both worlds
    controller_20->send_control_event(iso15118::d20::AuthorizationResponse{authorized});
    controller_2->send_control_event(iso15118::d2::AuthorizationResponse{authorized});
}

void ISO15118_chargerImpl::handle_ac_contactor_closed(bool& status) {
    // your code for cmd ac_contactor_closed goes here
}

void ISO15118_chargerImpl::handle_dlink_ready(bool& value) {
    // your code for cmd dlink_ready goes here
}

void ISO15118_chargerImpl::handle_cable_check_finished(bool& status) {
    //RDB Send to both worlds
    controller_20->send_control_event(iso15118::d20::CableCheckFinished{status});
    controller_2->send_control_event(iso15118::d2::CableCheckFinished{status});
}

void ISO15118_chargerImpl::handle_receipt_is_required(bool& receipt_required) {
    // your code for cmd receipt_is_required goes here
}

void ISO15118_chargerImpl::handle_stop_charging(bool& stop) {
    // your code for cmd stop_charging goes here
}

void ISO15118_chargerImpl::handle_update_ac_max_current(double& max_current) {
    // your code for cmd update_ac_max_current goes here
}

void ISO15118_chargerImpl::handle_update_dc_maximum_limits(
    types::iso15118_charger::DC_EVSEMaximumLimits& maximum_limits) {
    // your code for cmd update_dc_maximum_limits goes here
}

void ISO15118_chargerImpl::handle_update_dc_minimum_limits(
    types::iso15118_charger::DC_EVSEMinimumLimits& minimum_limits) {
    // your code for cmd update_dc_minimum_limits goes here
}

void ISO15118_chargerImpl::handle_update_isolation_status(types::iso15118_charger::IsolationStatus& isolation_status) {
    // your code for cmd update_isolation_status goes here
}

void ISO15118_chargerImpl::handle_update_dc_present_values(
    types::iso15118_charger::DC_EVSEPresentVoltage_Current& present_voltage_current) {

    float voltage = present_voltage_current.EVSEPresentVoltage;
    float current = 0;
    if (present_voltage_current.EVSEPresentCurrent.has_value()) {
        current = present_voltage_current.EVSEPresentCurrent.value();
    }
    //RDB Send to both worlds
    controller_20->send_control_event(iso15118::d20::PresentVoltageCurrent{voltage, current});
    controller_2->send_control_event(iso15118::d2::PresentVoltageCurrent{voltage, current});
}

void ISO15118_chargerImpl::handle_update_meter_info(types::powermeter::Powermeter& powermeter) {
    // your code for cmd update_meter_info goes here
}

void ISO15118_chargerImpl::handle_send_error(types::iso15118_charger::EvseError& error) {
    // your code for cmd send_error goes here
}

void ISO15118_chargerImpl::handle_reset_error() {
    // your code for cmd reset_error goes here
}

} // namespace charger
} // namespace module
