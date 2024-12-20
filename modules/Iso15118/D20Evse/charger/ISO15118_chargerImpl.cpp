// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "ISO15118_chargerImpl.hpp"

#include "session_logger.hpp"

#include <iso15118/io/logging.hpp>
#include <iso15118/session/feedback.hpp>
#include <iso15118/session/logger.hpp>
#include <iso15118/tbd_controller.hpp>

std::unique_ptr<iso15118::TbdController> controller;

iso15118::TbdConfig tbd_config;
iso15118::session::feedback::Callbacks callbacks;

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

    callbacks.dc_charge_target = [this](const iso15118::session::feedback::DcChargeTarget& charge_target) {
        types::iso15118_charger::DC_EVTargetValues tmp;
        tmp.DC_EVTargetVoltage = charge_target.voltage;
        tmp.DC_EVTargetCurrent = charge_target.current;
        publish_DC_EVTargetVoltageCurrent(tmp);
    };

    callbacks.dc_max_limits = [this](const iso15118::session::feedback::DcMaximumLimits& max_limits) {
        types::iso15118_charger::DC_EVMaximumLimits tmp;
        tmp.DC_EVMaximumVoltageLimit = max_limits.voltage;
        tmp.DC_EVMaximumCurrentLimit = max_limits.current;
        tmp.DC_EVMaximumPowerLimit = max_limits.power;
        publish_DC_EVMaximumLimits(tmp);
    };

    callbacks.signal = [this](iso15118::session::feedback::Signal signal) {
        using Signal = iso15118::session::feedback::Signal;
        switch (signal) {
        case Signal::CHARGE_LOOP_STARTED:
            publish_currentDemand_Started(nullptr);
            break;
        case Signal::SETUP_FINISHED:
            publish_V2G_Setup_Finished(nullptr);
            break;
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

    callbacks.v2g_message = [this](iso15118::session::feedback::V2gMessageId id) {
        types::iso15118_charger::V2G_Messages tmp;
        tmp.V2G_Message_ID = static_cast<types::iso15118_charger::V2G_Message_ID>(id);
        publish_V2G_Messages(tmp);
    };

    const auto default_cert_path = mod->info.paths.etc / "certs";

    const auto cert_path = get_cert_path(default_cert_path, mod->config.certificate_path);

    tbd_config = {
        {
            iso15118::config::CertificateBackend::EVEREST_LAYOUT,
            cert_path.string(),
            mod->config.private_key_password,
            mod->config.enable_ssl_logging,
        },
        mod->config.device,
        convert_tls_negotiation_strategy(mod->config.tls_negotiation_strategy),
    };

    controller = std::make_unique<iso15118::TbdController>(tbd_config, callbacks);

    // RDB subscribe to the position info from the PPD

    mod->r_PPD->subscribe_PositionMeasurement([this](types::pairing_and_positioning::Position uwb_position) {
        // Received UWB position values. Check if the UWB of the vehicle is within the Communications Pairing Space
        // (CPS) in Config. If so, Send the result over to the TbdController. Note that if the position is -100.0
        // cm, then the PPD is not receiving any signal from vehicle UWB. This can be treated the same as not within
        // the CPS. Also, ignore any readings with negative distance.
        if (uwb_position.position_X >= mod->config.communications_pairing_space_xmin &&
            uwb_position.position_X <= mod->config.communications_pairing_space_xmax &&
            uwb_position.position_Y >= mod->config.communications_pairing_space_ymin &&
            uwb_position.position_Y <= mod->config.communications_pairing_space_ymax &&
            uwb_position.position_Z >= 0.0) {
            controller->Is_PPD_in_CPS = true;
        } else {
            controller->Is_PPD_in_CPS = false;
        }

        // Calculate if the ev is in position
        bool ev_in_charge_position = false;

        if (uwb_position.position_X >= -mod->config.acdp_contact_window_xc &&
            uwb_position.position_X <= mod->config.acdp_contact_window_xc &&
            uwb_position.position_Y >= -mod->config.acdp_contact_window_yc &&
            uwb_position.position_Y <= mod->config.acdp_contact_window_yc && uwb_position.position_Z >= 0.0) {
            ev_in_charge_position = true;
        }

        // Also send a control event so that the vehicle positioning state handler gets it as well.
        // but only if the position info is good, though we probably need to send a control event if the PPD
        // signal is lost.
        if (uwb_position.position_Z >= 0.0) {
            controller->send_control_event(iso15118::d20::PresentVehiclePosition{
                mod->config.acdp_evse_positioning_support, (short)uwb_position.position_X,
                (short)uwb_position.position_Y, (short)mod->config.acdp_contact_window_xc,
                (short)mod->config.acdp_contact_window_yc, ev_in_charge_position});
        }
    });
}

void ISO15118_chargerImpl::ready() {
    // FIXME (aw): this is just plain stupid ...
    while (true) {
        try {
            controller->loop();
        } catch (const std::exception& e) {
            EVLOG_error << "D20Evse crashed: " << e.what();
            publish_dlink_error(nullptr);
        }

        controller.reset();

        const auto RETRY_INTERVAL = std::chrono::milliseconds(1000);

        EVLOG_info << "Trying to restart in " << std::to_string(RETRY_INTERVAL.count()) << " milliseconds";

        std::this_thread::sleep_for(RETRY_INTERVAL);

        controller = std::make_unique<iso15118::TbdController>(tbd_config, callbacks);
    }
}

void ISO15118_chargerImpl::handle_setup(
    types::iso15118_charger::EVSEID& evse_id,
    std::vector<types::iso15118_charger::EnergyTransferMode>& supported_energy_transfer_modes,
    types::iso15118_charger::SAE_J2847_Bidi_Mode& sae_j2847_mode, bool& debug_mode, bool& bidirectional) {

    std::vector<iso15118::message_20::ServiceCategory> supported_energy_transfer_services;

    for (auto mode : supported_energy_transfer_modes) {
        if (mode == types::iso15118_charger::EnergyTransferMode::AC_single_phase_core ||
            mode == types::iso15118_charger::EnergyTransferMode::AC_three_phase_core) {
            supported_energy_transfer_services.push_back(iso15118::message_20::ServiceCategory::AC);
            if (bidirectional) {
                supported_energy_transfer_services.push_back(iso15118::message_20::ServiceCategory::AC_BPT);
            }
        } else if (mode == types::iso15118_charger::EnergyTransferMode::DC_core ||
                   mode == types::iso15118_charger::EnergyTransferMode::DC_extended ||
                   mode == types::iso15118_charger::EnergyTransferMode::DC_combo_core ||
                   mode == types::iso15118_charger::EnergyTransferMode::DC_unique) {
            supported_energy_transfer_services.push_back(iso15118::message_20::ServiceCategory::DC);
            if (bidirectional) {
                supported_energy_transfer_services.push_back(iso15118::message_20::ServiceCategory::DC_BPT);
            }
        }
    }

    // TODO(sl): Check if ISO-2 evse_id format is the same for ISO-20
    controller->setup_config(evse_id.EVSE_ID, supported_energy_transfer_services);
}

void ISO15118_chargerImpl::handle_set_charging_parameters(
    types::iso15118_charger::SetupPhysicalValues& physical_values) {
    // TODO(SL)
}

void ISO15118_chargerImpl::handle_session_setup(std::vector<types::iso15118_charger::PaymentOption>& payment_options,
                                                bool& supported_certificate_service) {

    std::vector<iso15118::message_20::Authorization> auth_services;

    // Note(sl): Right now only eim is supported!
    for (auto& option : payment_options) {
        if (option == types::iso15118_charger::PaymentOption::ExternalPayment) {
            auth_services.push_back(iso15118::message_20::Authorization::EIM);
        }
        // } else if (option == types::iso15118_charger::PaymentOption::Contract) {
        //     auth_services.push_back(iso15118::message_20::Authorization::PnC);
        // }
    }

    controller->setup_session(auth_services, supported_certificate_service);
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

    controller->send_control_event(iso15118::d20::AuthorizationResponse{authorized});
}

void ISO15118_chargerImpl::handle_ac_contactor_closed(bool& status) {
    // your code for cmd ac_contactor_closed goes here
}

void ISO15118_chargerImpl::handle_dlink_ready(bool& value) {
    // your code for cmd dlink_ready goes here
}

void ISO15118_chargerImpl::handle_cable_check_finished(bool& status) {
    controller->send_control_event(iso15118::d20::CableCheckFinished{status});
}

void ISO15118_chargerImpl::handle_receipt_is_required(bool& receipt_required) {
    // your code for cmd receipt_is_required goes here
}

void ISO15118_chargerImpl::handle_stop_charging(bool& stop) {
    controller->send_control_event(iso15118::d20::StopCharging{stop});
}

void ISO15118_chargerImpl::handle_update_ac_max_current(double& max_current) {
    // send_control_event
}

void ISO15118_chargerImpl::handle_update_dc_maximum_limits(
    types::iso15118_charger::DC_EVSEMaximumLimits& maximum_limits) {
    controller->update_dc_max_values(maximum_limits.maximum_power, maximum_limits.maximum_current,
                                     maximum_limits.maximum_voltage, maximum_limits.maximum_discharge_power,
                                     maximum_limits.maximum_discharge_current);
}

void ISO15118_chargerImpl::handle_update_dc_minimum_limits(
    types::iso15118_charger::DC_EVSEMinimumLimits& minimum_limits) {

    controller->update_dc_min_values(minimum_limits.minimum_power, minimum_limits.minimum_current,
                                     minimum_limits.minimum_voltage, minimum_limits.minimum_discharge_power,
                                     minimum_limits.minimum_discharge_current);
}

// Note: Not used here
void ISO15118_chargerImpl::handle_update_isolation_status(types::iso15118_charger::IsolationStatus& isolation_status) {
}

void ISO15118_chargerImpl::handle_update_dc_present_values(
    types::iso15118_charger::DC_EVSEPresentVoltage_Current& present_voltage_current) {

    float voltage = present_voltage_current.EVSEPresentVoltage;
    float current = present_voltage_current.EVSEPresentCurrent.value_or(0);

    controller->send_control_event(iso15118::d20::PresentVoltageCurrent{voltage, current});
}

void ISO15118_chargerImpl::handle_update_meter_info(types::powermeter::Powermeter& powermeter) {
    // your code for cmd update_meter_info goes here
}

void ISO15118_chargerImpl::handle_send_error(types::iso15118_charger::EvseError& error) {
    // TODO(SL)
}

void ISO15118_chargerImpl::handle_reset_error() {
    // TODO(SL)
}

} // namespace charger
} // namespace module
