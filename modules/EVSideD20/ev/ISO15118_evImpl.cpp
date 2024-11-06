// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "ISO15118_evImpl.hpp"

#include "session_logger.hpp"
#include <eviso15118/io/logging.hpp>
#include <eviso15118/session/feedback.hpp>
#include <eviso15118/session/logger.hpp>
#include <eviso15118/tbd_controller.hpp>

eviso15118::TbdConfig tbd_config_20;
std::unique_ptr<SessionLogger> session_logger;
eviso15118::session::feedback::Callbacks callbacks_20;
std::unique_ptr<eviso15118::TbdController> controller_20;

namespace module {
namespace ev {

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

static eviso15118::config::TlsNegotiationStrategy convert_tls_negotiation_strategy(const std::string& strategy) {
    using Strategy = eviso15118::config::TlsNegotiationStrategy;
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

void ISO15118_evImpl::init() {
    // RDB TODO - No logging path in the ev interface...
    // setup logging routine
    eviso15118::io::set_logging_callback([](const std::string& msg) { EVLOG_info << msg; });

    // RDB use a temp folder instead
    std::filesystem::path p = std::filesystem::temp_directory_path();
    std::string sa = p.string();
    session_logger = std::make_unique<SessionLogger>(sa);

    //Get the config items from the calling module
    const auto default_cert_path = mod->info.paths.etc / "certs";

    //RDB TODO cert path not in interface nor is password, enable ssl logging or negotiation strategy
    //const auto cert_path = get_cert_path(default_cert_path, mod->config.certificate_path);
    //Send the SERVICE to the controller

    tbd_config_20 = {
        {
            eviso15118::config::CertificateBackend::EVEREST_LAYOUT,
            default_cert_path.string(),
            "123456",
            true,
            //mod->config.private_key_password,
            //mod->config.enable_ssl_logging,
        },
        mod->config.device,
        eviso15118::config::TlsNegotiationStrategy::ENFORCE_TLS,
    };


    // RDB First we do the ISO20. This constructor causes everything to become ready, but
    // waiting for a first StartCharging message to come in.

    controller_20 = std::make_unique<eviso15118::TbdController>(tbd_config_20, callbacks_20);
}

void ISO15118_evImpl::ready() {
    // start looping waiting for messages and control events to come in
    while (true) {
        try {
            controller_20->loop();
        } catch (const std::exception& e) {
            EVLOG_error << "D20 EV crashed: " << e.what();
            //publish_dlink_error(nullptr);
        }
    }
}

bool ISO15118_evImpl::handle_start_charging(types::iso15118_ev::EnergyTransferMode& EnergyTransferMode) {
    // We need to be already connected to a wireless network using ISO15118-8
    // Similar to SLAC, the wireless connection could be done by the iso15118_WiFi module, and this module is informed 
    // asynchronously by mqtt
    // For now, assume we are connected.

    // Start the charging process (including SDP, either wireless or normal)
    // This will be handled in the first state of the state machine on startup (or restart)
    eviso15118::d20::start_stop_charging ss = eviso15118::d20::start_stop_charging::START_CHARGING;
    controller_20->send_control_event(eviso15118::d20::StartStopCharging{ss});

    return true;
}

void ISO15118_evImpl::handle_stop_charging() {
    // your code for cmd stop_charging goes here
    eviso15118::d20::start_stop_charging ss = eviso15118::d20::start_stop_charging::STOP_CHARGING;
    controller_20->send_control_event(eviso15118::d20::StartStopCharging{ss});
    return;
}

void ISO15118_evImpl::handle_pause_charging() {
    // your code for cmd pause_charging goes here
    eviso15118::d20::start_stop_charging ss = eviso15118::d20::start_stop_charging::PAUSE_CHARGING;
    controller_20->send_control_event(eviso15118::d20::StartStopCharging{ss});
    return;    
}

void ISO15118_evImpl::handle_set_fault() {
    // your code for cmd set_fault goes here
}

void ISO15118_evImpl::handle_set_dc_params(types::iso15118_ev::DC_EVParameters& EV_Parameters) {
    // your code for cmd set_dc_params goes here
}

void ISO15118_evImpl::handle_set_bpt_dc_params(types::iso15118_ev::DC_EV_BPT_Parameters& EV_BPT_Parameters) {
    // your code for cmd set_bpt_dc_params goes here
}

void ISO15118_evImpl::handle_enable_sae_j2847_v2g_v2h() {
    // your code for cmd enable_sae_j2847_v2g_v2h goes here
}

} // namespace ev
} // namespace module
