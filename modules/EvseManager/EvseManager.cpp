// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "EvseManager.hpp"
#include "SessionLog.hpp"
#include "Timeout.hpp"
#include <fmt/color.h>
#include <fmt/core.h>
using namespace std::literals::chrono_literals;

namespace module {

void EvseManager::init() {
    local_three_phases = config.three_phases;

    session_log.setPath(config.session_logging_path);
    if (config.session_logging) {
        session_log.enable();
    }
    session_log.xmlOutput(config.session_logging_xml);

    invoke_init(*p_evse);
    invoke_init(*p_energy_grid);
    invoke_init(*p_token_provider);

    // check if a slac module is connected to the optional requirement
    slac_enabled = !r_slac.empty();

    // if hlc is disabled in config, disable slac even if requirement is connected
    if (!(config.ac_hlc_enabled || config.ac_with_soc || config.charge_mode == "DC")) {
        slac_enabled = false;
    }
    hlc_enabled = !r_hlc.empty();

    if (config.charge_mode == "DC" && (!hlc_enabled || !slac_enabled || r_powersupply_DC.empty())) {
        EVLOG_error << "DC mode requires slac, HLC and powersupply DCDC to be connected";
        exit(255);
    }

    if (config.charge_mode == "DC" && r_imd.empty()) {
        EVLOG_warning << "DC mode without isolation monitoring configured, please check your national regulations.";
    }

    reserved = false;
    reservation_id = 0;

    hlc_waiting_for_auth_eim = false;
    hlc_waiting_for_auth_pnc = false;

    latest_target_voltage = 0;
    latest_target_current = 0;
}

void EvseManager::ready() {
    charger = std::unique_ptr<Charger>(new Charger(r_bsp));

    if (get_hlc_enabled()) {

        // Set up EVSE ID
        r_hlc[0]->call_set_EVSEID(config.evse_id);

        // Set up auth options for HLC
        Array payment_options;
        if (config.payment_enable_eim) {
            payment_options.insert(payment_options.end(), "ExternalPayment");
        }
        if (config.payment_enable_contract) {
            payment_options.insert(payment_options.end(), "Contract");
        }
        r_hlc[0]->call_set_PaymentOptions(payment_options);

        // Set up energy transfer modes for HLC. For now we only support either DC or AC, not both at the same time.
        Array transfer_modes;
        if (config.charge_mode == "AC") {
            r_hlc[0]->call_set_AC_EVSENominalVoltage(config.ac_nominal_voltage);

            if (config.three_phases) {
                transfer_modes.insert(transfer_modes.end(), "AC_three_phase_core");
                transfer_modes.insert(transfer_modes.end(), "AC_single_phase_core");
            } else {
                transfer_modes.insert(transfer_modes.end(), "AC_single_phase_core");
            }
        } else if (config.charge_mode == "DC") {
            // transfer_modes.insert(transfer_modes.end(), "DC_core");
            transfer_modes.insert(transfer_modes.end(), "DC_extended");

            powersupply_capabilities = r_powersupply_DC[0]->call_getCapabilities();

            r_hlc[0]->call_set_DC_EVSECurrentRegulationTolerance(
                powersupply_capabilities.current_regulation_tolerance_A);
            r_hlc[0]->call_set_DC_EVSEPeakCurrentRipple(powersupply_capabilities.peak_current_ripple_A);
            r_hlc[0]->call_set_DC_EVSEPresentVoltage(0);
            r_hlc[0]->call_set_DC_EVSEPresentCurrent(0);
            r_hlc[0]->call_set_EVSEEnergyToBeDelivered(10000);

            types::iso15118_charger::DC_EVSEMaximumLimits evseMaxLimits;
            evseMaxLimits.EVSEMaximumCurrentLimit = powersupply_capabilities.max_export_current_A;
            evseMaxLimits.EVSEMaximumPowerLimit = powersupply_capabilities.max_export_power_W;
            evseMaxLimits.EVSEMaximumVoltageLimit = powersupply_capabilities.max_export_voltage_V;
            r_hlc[0]->call_set_DC_EVSEMaximumLimits(evseMaxLimits);

            types::iso15118_charger::DC_EVSEMinimumLimits evseMinLimits;
            evseMinLimits.EVSEMinimumCurrentLimit = powersupply_capabilities.min_export_current_A;
            evseMinLimits.EVSEMinimumVoltageLimit = powersupply_capabilities.min_export_voltage_V;
            r_hlc[0]->call_set_DC_EVSEMinimumLimits(evseMinLimits);

            // Cable check for DC charging
            r_hlc[0]->subscribe_Start_CableCheck([this] { cable_check(); });

            // Notification that current demand has started
            r_hlc[0]->subscribe_currentDemand_Started([this] { charger->notifyCurrentDemandStarted(); });

            // Isolation monitoring for DC charging handler
            if (!r_imd.empty()) {
                r_imd[0]->subscribe_IsolationMeasurement([this](types::isolation_monitor::IsolationMeasurement m) {
                    // new DC isolation monitoring measurement received
                    EVLOG_info << fmt::format("Isolation measurement P {} N {}.", m.resistance_P_Ohm,
                                              m.resistance_N_Ohm);
                    isolation_measurement = m;
                });
            }

            // Get voltage/current from DC power supply
            if (!r_powersupply_DC.empty()) {
                r_powersupply_DC[0]->subscribe_voltage_current([this](types::power_supply_DC::VoltageCurrent m) {
                    powersupply_measurement = m;
                    r_hlc[0]->call_set_DC_EVSEPresentCurrent((m.current_A > 0 ? m.current_A : 0.0));
                    r_hlc[0]->call_set_DC_EVSEPresentVoltage((m.voltage_V > 0 ? m.voltage_V : 0.0));
                });
            }

            // Car requests a target voltage and current limit
            r_hlc[0]->subscribe_DC_EVTargetVoltageCurrent([this](types::iso15118_charger::DC_EVTargetValues v) {
                bool target_changed = false;

                if (v.DC_EVTargetVoltage != latest_target_voltage || v.DC_EVTargetCurrent != latest_target_current) {
                    latest_target_voltage = v.DC_EVTargetVoltage;
                    latest_target_current = v.DC_EVTargetCurrent;
                    target_changed = true;
                }
                if (target_changed) {
                    powersupply_DC_set(v.DC_EVTargetVoltage, v.DC_EVTargetCurrent);
                    if (!contactor_open) {
                        powersupply_DC_on();
                    }
                }
            });

            // Car requests DC contactor open. We don't actually open but switch off DC supply.
            // opening will be done by Charger on C->B CP event.
            r_hlc[0]->subscribe_DC_Open_Contactor([this](bool b) {
                if (b)
                    powersupply_DC_off();
                r_imd[0]->call_stop();
            });

            // Back up switch off - charger signalled that it needs to switch off now.
            // During normal operation this should be done earlier before switching off relais by HLC protocol.
            charger->signal_DC_supply_off.connect([this] {
                powersupply_DC_off();
                r_imd[0]->call_stop();
            });

            // Current demand has finished - switch off DC supply
            r_hlc[0]->subscribe_currentDemand_Finished([this] { powersupply_DC_off(); });

            // unused vars of HLC for now:
            // DC_EVMaximumLimits

            // AC_Close_Contactor
            // AC_Open_Contactor

            // V2G_Setup_Finished
            // SelectedPaymentOption
            // RequestedEnergyTransferMode
            // DepartureTime
            // AC_EAmount
            // AC_EVMaxVoltage
            // AC_EVMaxCurrent
            // AC_EVMinCurrent
            // DC_EVEnergyCapacity
            // DC_EVEnergyRequest
            // DC_FullSOC
            // DC_BulkSOC
            // DC_EVStatus
            // EV_ChargingSession
            // DC_BulkChargingComplete
            // DC_ChargingComplete
            // DC_EVRemainingTime

        } else {
            EVLOG_error << "Unsupported charging mode.";
            exit(255);
        }
        r_hlc[0]->call_set_SupportedEnergyTransferMode(transfer_modes);

        r_hlc[0]->call_set_ReceiptRequired(config.ev_receipt_required);

        // We always go through the auth step with EIM even if it is free. This way EVerest auth manager has full
        // control.
        r_hlc[0]->call_set_FreeService(false);

        // set up debug mode for HLC
        if (config.session_logging) {
            types::iso15118_charger::DebugMode debug_mode = types::iso15118_charger::DebugMode::Full;
            r_hlc[0]->call_enable_debug_mode(debug_mode);
        }

        // reset error flags
        r_hlc[0]->call_set_FAILED_ContactorError(false);
        r_hlc[0]->call_set_RCD_Error(false);

        // implement Auth handlers
        r_hlc[0]->subscribe_Require_Auth_EIM([this]() {
            // EVLOG_info << "--------------------------------------------- Require_Auth_EIM called";
            //  Do we have auth already (i.e. delayed HLC after charging already running)?
            if ((config.dbg_hlc_auth_after_tstep && charger->Authorized_EIM_ready_for_HLC()) ||
                (!config.dbg_hlc_auth_after_tstep && charger->Authorized_EIM())) {
                {
                    std::scoped_lock lock(hlc_mutex);
                    hlc_waiting_for_auth_eim = false;
                    hlc_waiting_for_auth_pnc = false;
                }
                r_hlc[0]->call_set_Auth_Okay_EIM(true);
            } else {
                std::scoped_lock lock(hlc_mutex);
                hlc_waiting_for_auth_eim = true;
                hlc_waiting_for_auth_pnc = false;
            }
        });

        // implement Auth handlers
        r_hlc[0]->subscribe_EVCCIDD([this](const std::string& _token) {
            json autocharge_token;

            std::string token = _token;
            token.erase(remove(token.begin(), token.end(), ':'), token.end());
            autocharge_token["id_token"] = "VID:" + token;
            autocharge_token["type"] = "autocharge";
            autocharge_token["timeout"] = 60;

            p_token_provider->publish_provided_token(autocharge_token);
        });

        r_hlc[0]->subscribe_Require_Auth_PnC([this]() {
            // Do we have auth already (i.e. delayed HLC after charging already running)?
            if (charger->Authorized_PnC()) {
                {
                    std::scoped_lock lock(hlc_mutex);
                    hlc_waiting_for_auth_eim = false;
                    hlc_waiting_for_auth_pnc = false;
                }
                r_hlc[0]->call_set_Auth_Okay_PnC(true);
            } else {
                std::scoped_lock lock(hlc_mutex);
                hlc_waiting_for_auth_eim = false;
                hlc_waiting_for_auth_pnc = true;
            }
        });

        // Install debug V2G Messages handler if session logging is enabled
        if (config.session_logging) {
            r_hlc[0]->subscribe_V2G_Messages([this](types::iso15118_charger::V2G_Messages v2g_messages) {
                json v2g = v2g_messages;
                log_v2g_message(v2g);
            });
        }
        // switch to DC mode for first session for AC with SoC
        if (config.ac_with_soc) {

            r_bsp->subscribe_event([this](const types::board_support::Event& event) {
                if (event == types::board_support::Event::CarUnplugged) {
                    // configure for DC again for next session. Will reset to AC when SoC is received
                    switch_DC_mode();
                }
            });

            charger->signalACWithSoCTimeout.connect([this]() { switch_DC_mode(); });

            r_hlc[0]->subscribe_DC_EVStatus([this](types::iso15118_charger::DC_EVStatusType status) {
                EVLOG_info << fmt::format("SoC received: {}.", status.DC_EVRESSSOC);
                switch_AC_mode();
            });
        }
    }

    hw_capabilities = r_bsp->call_get_hw_capabilities();

    // Maybe override with user setting for this EVSE
    if (config.max_current < hw_capabilities.max_current_A) {
        hw_capabilities.max_current_A = config.max_current;
    }

    local_max_current_limit = hw_capabilities.max_current_A;

    // Maybe limit to single phase by user setting if possible with HW
    if (!config.three_phases && hw_capabilities.min_phase_count == 1) {
        hw_capabilities.max_phase_count = 1;
        local_three_phases = false;
    } else if (hw_capabilities.max_phase_count == 3) {
        local_three_phases = true; // other configonfigurations currently not supported by HW
    }

    r_bsp->subscribe_event([this](const types::board_support::Event event) {
        charger->processEvent(event);

        // Forward some events to HLC
        if (get_hlc_enabled()) {
            // Reset HLC auth waiting flags on new session
            if (event == types::board_support::Event::CarPluggedIn) {
                r_hlc[0]->call_set_FAILED_ContactorError(false);
                r_hlc[0]->call_set_RCD_Error(false);
                r_hlc[0]->call_set_EVSE_Malfunction(false);
                r_hlc[0]->call_set_EVSE_EmergencyShutdown(false);
                r_hlc[0]->call_contactor_open(true);
                r_hlc[0]->call_stop_charging(false);
                latest_target_voltage = 0;
                latest_target_current = 0;
                {
                    std::scoped_lock lock(hlc_mutex);
                    hlc_waiting_for_auth_eim = false;
                    hlc_waiting_for_auth_pnc = false;
                }
            }

            if (event == types::board_support::Event::ErrorRelais) {
                session_log.evse(false, "Error Relais");
                r_hlc[0]->call_set_FAILED_ContactorError(true);
            }

            if (event == types::board_support::Event::ErrorRCD) {
                session_log.evse(false, "Error RCD");
                r_hlc[0]->call_set_RCD_Error(true);
            }

            if (event == types::board_support::Event::PermanentFault) {
                session_log.evse(false, "Error Permanent Fault");
                r_hlc[0]->call_set_EVSE_Malfunction(true);
            }

            if (event == types::board_support::Event::ErrorOverCurrent) {
                session_log.evse(false, "Error Over Current");
                r_hlc[0]->call_set_EVSE_EmergencyShutdown(true);
            }

            if (event == types::board_support::Event::PowerOn) {
                contactor_open = false;
                r_hlc[0]->call_contactor_closed(true);
            }

            if (event == types::board_support::Event::PowerOff) {
                contactor_open = true;
                r_hlc[0]->call_contactor_open(true);
            }
        }

        // Forward events from BSP to SLAC module
        if (slac_enabled) {
            if (event == types::board_support::Event::EnterBCD) {
                r_slac[0]->call_enter_bcd();
            } else if (event == types::board_support::Event::LeaveBCD) {
                r_slac[0]->call_leave_bcd();
            } else if (event == types::board_support::Event::CarPluggedIn) {
                r_slac[0]->call_reset(true);
            } else if (event == types::board_support::Event::CarUnplugged) {
                r_slac[0]->call_reset(false);
            }
        }

        if (config.ac_with_soc)
            charger->signalACWithSoCTimeout.connect([this]() {
                EVLOG_info << "AC with SoC timeout";
                switch_DC_mode();
            });
    });

    r_bsp->subscribe_nr_of_phases_available([this](int n) { signalNrOfPhasesAvailable(n); });

    if (r_powermeter.size() > 0) {
        r_powermeter[0]->subscribe_powermeter([this](types::powermeter::Powermeter powermeter) {
            json p = powermeter;
            // Inform charger about current charging current. This is used for slow OC detection.
            charger->setCurrentDrawnByVehicle(p["current_A"]["L1"], p["current_A"]["L2"], p["current_A"]["L3"]);

            // Inform HLC about the power meter data
            if (get_hlc_enabled()) {
                r_hlc[0]->call_set_MeterInfo(p);
            }

            // Store local cache
            latest_powermeter_data = p;

            // External Nodered interface
            mqtt.publish(fmt::format("everest_external/nodered/{}/powermeter/phaseSeqError", config.connector_id),
                         powermeter.phase_seq_error.get());
            mqtt.publish(fmt::format("everest_external/nodered/{}/powermeter/time_stamp", config.connector_id),
                         powermeter.timestamp);
            mqtt.publish(fmt::format("everest_external/nodered/{}/powermeter/totalKw", config.connector_id),
                         (powermeter.power_W.get().L1.get() + powermeter.power_W.get().L2.get() +
                          powermeter.power_W.get().L3.get()) /
                             1000.,
                         1);
            mqtt.publish(fmt::format("everest_external/nodered/{}/powermeter/totalKWattHr", config.connector_id),
                         powermeter.energy_Wh_import.total / 1000.);
            mqtt.publish(fmt::format("everest_external/nodered/{}/powermeter_json", config.connector_id), p.dump());
            // /External Nodered interface
        });
    }

    if (slac_enabled) {
        r_slac[0]->subscribe_state([this](const std::string& s) {
            session_log.evse(true, fmt::format("SLAC {}", s));
            // Notify charger whether matching was started (or is done) or not
            if (s == "UNMATCHED") {
                charger->setMatchingStarted(false);
            } else {
                charger->setMatchingStarted(true);
            }
        });
    }

    charger->signalMaxCurrent.connect([this](float ampere) {
        // The charger changed the max current setting. Forward to HLC
        if (get_hlc_enabled()) {
            r_hlc[0]->call_set_AC_EVSEMaxCurrent(ampere);
        }
    });

    if (slac_enabled) {
        r_slac[0]->subscribe_request_error_routine([this]() {
            EVLOG_info << "Received request error routine from SLAC in evsemanager\n";
            charger->requestErrorSequence();
        });
    }
    charger->signalEvent.connect([this](types::evse_manager::SessionEventEnum s) {
        // Cancel reservations if charger is disabled or faulted
        if (s == types::evse_manager::SessionEventEnum::Disabled ||
            s == types::evse_manager::SessionEventEnum::PermanentFault) {
            cancel_reservation();
        }
    });

    invoke_ready(*p_evse);
    invoke_ready(*p_energy_grid);
    invoke_ready(*p_token_provider);
    if (config.ac_with_soc) {
        setup_fake_DC_mode();
    } else {
        charger->setup(local_three_phases, config.has_ventilation, config.country_code, config.rcd_enabled,
                       (config.charge_mode == "DC" ? Charger::ChargeMode::DC : Charger::ChargeMode::AC), slac_enabled,
                       config.ac_hlc_use_5percent, config.ac_enforce_hlc, false);
    }
    //  start with a limit of 0 amps. We will get a budget from EnergyManager that is locally limited by hw
    //  caps.
    charger->setMaxCurrent(0.0F, date::utc_clock::now());
    charger->run();
    charger->enable();

    EVLOG_info << fmt::format(fmt::emphasis::bold | fg(fmt::terminal_color::green), ">>> Ready to start charging <<<");
}

json EvseManager::get_latest_powermeter_data() {
    return latest_powermeter_data;
}

types::board_support::HardwareCapabilities EvseManager::get_hw_capabilities() {
    return hw_capabilities;
}

int32_t EvseManager::get_reservation_id() {
    std::lock_guard<std::mutex> lock(reservation_mutex);
    return reservation_id;
}

void EvseManager::switch_DC_mode() {
    charger->evseReplug();
    setup_fake_DC_mode();
}

void EvseManager::switch_AC_mode() {
    charger->evseReplug();
    setup_AC_mode();
}

// This sets up a fake DC mode that is just supposed to work until we get the SoC.
// It is only used for AC<>DC<>AC<>DC mode to get AC charging with SoC.
void EvseManager::setup_fake_DC_mode() {
    charger->setup(local_three_phases, config.has_ventilation, config.country_code, config.rcd_enabled,
                   Charger::ChargeMode::DC, slac_enabled, config.ac_hlc_use_5percent, config.ac_enforce_hlc, false);

    // Set up energy transfer modes for HLC. For now we only support either DC or AC, not both at the same time.
    Array transfer_modes;
    transfer_modes.insert(transfer_modes.end(), "DC_core");
    transfer_modes.insert(transfer_modes.end(), "DC_extended");
    transfer_modes.insert(transfer_modes.end(), "DC_combo_core");
    transfer_modes.insert(transfer_modes.end(), "DC_unique");
    r_hlc[0]->call_set_DC_EVSECurrentRegulationTolerance(config.dc_current_regulation_tolerance);
    r_hlc[0]->call_set_DC_EVSEPeakCurrentRipple(config.dc_peak_current_ripple);
    r_hlc[0]->call_set_DC_EVSEPresentVoltage(400); // FIXME: set a correct values
    r_hlc[0]->call_set_DC_EVSEPresentCurrent(0);

    types::iso15118_charger::DC_EVSEMaximumLimits evseMaxLimits;
    evseMaxLimits.EVSEMaximumCurrentLimit = 400;
    evseMaxLimits.EVSEMaximumPowerLimit = 200000;
    evseMaxLimits.EVSEMaximumVoltageLimit = 1000;
    r_hlc[0]->call_set_DC_EVSEMaximumLimits(evseMaxLimits);

    types::iso15118_charger::DC_EVSEMinimumLimits evseMinLimits;
    evseMinLimits.EVSEMinimumCurrentLimit = 0;
    evseMinLimits.EVSEMinimumVoltageLimit = 0;
    r_hlc[0]->call_set_DC_EVSEMinimumLimits(evseMinLimits);

    r_hlc[0]->call_set_SupportedEnergyTransferMode(transfer_modes);
}

void EvseManager::setup_AC_mode() {
    charger->setup(local_three_phases, config.has_ventilation, config.country_code, config.rcd_enabled,
                   Charger::ChargeMode::AC, slac_enabled, config.ac_hlc_use_5percent, config.ac_enforce_hlc, true);

    // Set up energy transfer modes for HLC. For now we only support either DC or AC, not both at the same time.
    Array transfer_modes;
    if (config.three_phases) {
        transfer_modes.insert(transfer_modes.end(), "AC_three_phase_core");
        transfer_modes.insert(transfer_modes.end(), "AC_single_phase_core");
    } else {
        transfer_modes.insert(transfer_modes.end(), "AC_single_phase_core");
    }
    if (get_hlc_enabled())
        r_hlc[0]->call_set_SupportedEnergyTransferMode(transfer_modes);
}

bool EvseManager::updateLocalMaxCurrentLimit(float max_current) {
    if (max_current >= 0.0F && max_current < EVSE_ABSOLUTE_MAX_CURRENT) {
        local_max_current_limit = max_current;

        // wait for EnergyManager to assign optimized current on next opimizer run

        return true;
    }
    return false;
}

bool EvseManager::reserve(int32_t id) {

    // is the evse Unavailable?
    if (charger->getCurrentState() == Charger::EvseState::Disabled) {
        return false;
    }

    // is the evse faulted?
    if (charger->getCurrentState() == Charger::EvseState::Faulted) {
        return false;
    }

    // is the connector currently ready to accept a new car?
    if (charger->getCurrentState() != Charger::EvseState::Idle) {
        return false;
    }

    std::lock_guard<std::mutex> lock(reservation_mutex);

    if (!reserved) {
        reserved = true;
        reservation_id = id;

        // publish event to other modules
        json se;
        se["event"] = "ReservationStart";

        signalReservationEvent(se);
        return true;
    }

    return false;
}

void EvseManager::cancel_reservation() {

    std::lock_guard<std::mutex> lock(reservation_mutex);
    if (reserved) {
        reserved = false;
        reservation_id = 0;

        // publish event to other modules
        json se;
        se["event"] = "ReservationEnd";

        signalReservationEvent(se);
    }
}

bool EvseManager::is_reserved() {
    std::lock_guard<std::mutex> lock(reservation_mutex);
    return reserved;
}

float EvseManager::getLocalMaxCurrentLimit() {
    return local_max_current_limit;
}

bool EvseManager::get_hlc_enabled() {
    std::lock_guard<std::mutex> lock(hlc_mutex);
    return hlc_enabled;
}

void EvseManager::log_v2g_message(Object m) {
    std::string msg = m["V2G_Message_ID"];

    std::string xml = "";
    std::string json_str = "";
    if (m["V2G_Message_XML"].is_null() && m["V2G_Message_JSON"].is_string()) {
        json_str = m["V2G_Message_JSON"];
    } else if (m["V2G_Message_XML"].is_string()) {
        xml = m["V2G_Message_XML"];
    }

    // All messages from EVSE contain Req and all originating from Car contain Res
    if (msg.find("Res") == std::string::npos) {
        session_log.car(true, fmt::format("V2G {}", msg), xml, m["V2G_Message_EXI_Hex"], m["V2G_Message_EXI_Base64"],
                        json_str);
    } else {
        session_log.evse(true, fmt::format("V2G {}", msg), xml, m["V2G_Message_EXI_Hex"], m["V2G_Message_EXI_Base64"],
                         json_str);
    }
}

void EvseManager::charger_was_authorized() {

    std::scoped_lock lock(hlc_mutex);
    if (hlc_waiting_for_auth_pnc && charger->Authorized_PnC()) {
        r_hlc[0]->call_set_Auth_Okay_PnC(true);
        hlc_waiting_for_auth_eim = false;
        hlc_waiting_for_auth_pnc = false;
    }

    if (hlc_waiting_for_auth_eim && charger->Authorized_EIM()) {
        r_hlc[0]->call_set_Auth_Okay_EIM(true);
        hlc_waiting_for_auth_eim = false;
        hlc_waiting_for_auth_pnc = false;
    }
}

void EvseManager::cable_check() {

    if (r_imd.empty()) {
        // If no IMD is connected, we skip isolation checking.
        EVLOG_info << "No IMD: skippint cable check.";
        r_hlc[0]->call_cableCheck_Finished(false);
        r_hlc[0]->call_set_EVSEIsolationStatus(types::iso15118_charger::IsolationStatus::No_IMD);
        return;
    }
    // start cable check in a seperate thread.
    std::thread t([this]() {
        session_log.evse(true, "Start cable check...");
        bool ok = false;

        // verify the relais are really switched on and set 500V output
        if (!contactor_open && powersupply_DC_set(500, 2)) {

            powersupply_DC_on();
            r_imd[0]->call_start();

            // wait until the voltage has rised to the target value
            if (!wait_powersupply_DC_voltage_reached(500)) {
                EVLOG_info << "Voltage did not rise to 500V within timeout";
                powersupply_DC_off();
                ok = false;
                r_imd[0]->call_stop();
            } else {
                // read out one new isolation resistance
                isolation_measurement.clear();
                types::isolation_monitor::IsolationMeasurement m;
                if (!isolation_measurement.wait_for(m, 10s)) {
                    EVLOG_info << "Did not receive isolation measurement from IMD within 10 seconds.";
                    powersupply_DC_off();
                    ok = false;
                } else {
                    // wait until the voltage is back to safe level
                    float minvoltage = powersupply_capabilities.min_export_voltage_V;

                    // We do not want to shut down power supply
                    if (minvoltage < 60) {
                        minvoltage = 60;
                    }
                    powersupply_DC_set(minvoltage, 2);

                    if (!wait_powersupply_DC_below_voltage(minvoltage + 20)) {
                        EVLOG_info << "Voltage did not go back to minimal voltage within timeout.";
                        ok = false;
                    } else {
                        // verify it is within ranges. Warning level is <500 Ohm/V_max_output_rating, Fault
                        // is <100
                        const double min_resistance_ok = 500. / powersupply_capabilities.max_export_voltage_V;
                        const double min_resistance_warning = 100. / powersupply_capabilities.max_export_voltage_V;

                        if (m.resistance_N_Ohm < min_resistance_warning ||
                            m.resistance_P_Ohm < min_resistance_warning) {
                            EVLOG_error << fmt::format("Isolation measurement FAULT P {} N {}.", m.resistance_P_Ohm,
                                                       m.resistance_N_Ohm);
                            ok = false;
                            r_hlc[0]->call_set_EVSEIsolationStatus(types::iso15118_charger::IsolationStatus::Fault);
                            r_imd[0]->call_stop();
                        } else if (m.resistance_N_Ohm < min_resistance_ok || m.resistance_P_Ohm < min_resistance_ok) {
                            EVLOG_error << fmt::format("Isolation measurement WARNING P {} N {}.", m.resistance_P_Ohm,
                                                       m.resistance_N_Ohm);
                            ok = true;
                            r_hlc[0]->call_set_EVSEIsolationStatus(types::iso15118_charger::IsolationStatus::Warning);
                        } else {
                            EVLOG_info << fmt::format("Isolation measurement Ok P {} N {}.", m.resistance_P_Ohm,
                                                      m.resistance_N_Ohm);
                            ok = true;
                            r_hlc[0]->call_set_EVSEIsolationStatus(types::iso15118_charger::IsolationStatus::Valid);
                        }
                    }
                }
            }
        }

        // submit result to HLC
        r_hlc[0]->call_cableCheck_Finished(ok);
    });
    // Detach thread and exit command handler right away
    t.detach();
}

void EvseManager::powersupply_DC_on() {
    r_powersupply_DC[0]->call_setMode(types::power_supply_DC::Mode::Export);
}

bool EvseManager::powersupply_DC_set(double voltage, double current) {
    // check limits of supply
    if (voltage >= powersupply_capabilities.min_export_voltage_V &&
        voltage <= powersupply_capabilities.max_export_voltage_V) {

        if (current > powersupply_capabilities.max_export_current_A)
            current = powersupply_capabilities.max_export_current_A;

        if (current < powersupply_capabilities.min_export_current_A)
            current = powersupply_capabilities.min_export_current_A;

        if (voltage * current > powersupply_capabilities.max_export_power_W)
            current = powersupply_capabilities.max_export_power_W / voltage;

        EVLOG_info << fmt::format("DC voltage/current set: Voltage {} Current {}.", voltage, current);

        r_powersupply_DC[0]->call_setExportVoltageCurrent(voltage, current);
        return true;
    }
    EVLOG_critical << fmt::format("DC voltage/current out of limits requested: Voltage {} Current {}.", voltage,
                                  current);
    return false;
}

void EvseManager::powersupply_DC_off() {
    r_powersupply_DC[0]->call_setMode(types::power_supply_DC::Mode::Off);
}

bool EvseManager::wait_powersupply_DC_voltage_reached(double target_voltage) {
    // wait until the voltage has rised to the target value
    Timeout timeout(30s);
    bool voltage_ok = false;
    while (!timeout.reached()) {
        types::power_supply_DC::VoltageCurrent m;
        if (powersupply_measurement.wait_for(m, 2000ms)) {
            if (fabs(m.voltage_V - target_voltage) < 10) {
                voltage_ok = true;
                break;
            }
        } else {
            EVLOG_info << "Did not receive voltage measurement from power supply within 2 seconds.";
            powersupply_DC_off();
            break;
        }
    }
    return voltage_ok;
}

bool EvseManager::wait_powersupply_DC_below_voltage(double target_voltage) {
    // wait until the voltage is below the target voltage
    Timeout timeout(30s);
    bool voltage_ok = false;
    while (!timeout.reached()) {
        types::power_supply_DC::VoltageCurrent m;
        if (powersupply_measurement.wait_for(m, 2000ms)) {
            if (m.voltage_V < target_voltage) {
                voltage_ok = true;
                break;
            }
        } else {
            EVLOG_info << "Did not receive voltage measurement from power supply within 2 seconds.";
            powersupply_DC_off();
            break;
        }
    }
    return voltage_ok;
}

} // namespace module
