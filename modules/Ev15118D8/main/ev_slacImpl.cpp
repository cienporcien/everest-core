// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "ev_slacImpl.hpp"
#include <filesystem>
#include <fmt/core.h>

//Nice way to know when to start running.
static std::promise<void> module_ready;

namespace module {
namespace main {

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


void ev_slacImpl::init() {
    // setup ev run thread
    std::thread(&ev_slacImpl::run, this).detach();
}

void ev_slacImpl::ready() {
    module_ready.set_value();
}

void ev_slacImpl::run() {
    // wait until ready
    module_ready.get_future().get();

    //The WLAN is controlled using the libwpa_client-dev library, which in turn has an 
    //example of how to access it in the wpa_cli.c source file available at https://w1.fi/hostap.git
    //Most things are taken from there.

    //Initialize the WLAN interface using the libwpa_client.a library
    std::string fullcs = "/var/run/wpa_supplicant/" + config.device;
    ctrl_conn = wpa_ctrl_open(fullcs.c_str());
	if (!ctrl_conn){
        //error
            std::string errstr = fmt::format("WPA_client couldn't open device {} for WiFi communication. Error: {}", config.device, strerror(errno));
            EVLOG_error << errstr;
            raise_error(error_factory->create_error("generic/CommunicationFault", "", errstr));
        return;
    }

    //Make sure the wpa_ctrl is working using PING
    const char* thecmd  = "PING";
    char reply[4096];
    size_t reply_len = sizeof(reply);
    int rc = wpa_ctrl_request(ctrl_conn, thecmd, strlen(thecmd), reply, &reply_len, NULL );
	std::string reply_str = reply;
    if(reply_str.substr(0, 4) != "PONG"){
        //error, close and leave
        std::string errstr = fmt::format("WPA_client couldn't ping device {} for WiFi communication. Error: {}", config.device, strerror(errno));
        EVLOG_error << errstr;
        raise_error(error_factory->create_error("generic/CommunicationFault", "", errstr));
        wpa_ctrl_close(ctrl_conn);
        return;
    }
    std::ostringstream oss;

    //Unique ISO Organization ID
    oss << "70b3d53190";
    //Element ID (EV VSE)
    oss << "02";
    //Form a byte for the Energy Transfer Type (ETT)
    if(config.ETT_AC){
        ETT = ETT | 0x01;
    }
    if(config.ETT_DC){
        ETT = ETT | 0x02;
    }
    if(config.ETT_WPT){
        ETT = ETT | 0x04;
    }
    if(config.ETT_ACD){
        ETT = ETT | 0x08;
    }
    int ti = ETT;
    //Convert to hex
    oss << std::hex << std::setfill('0') << std::setw(2) << ti;

    //The additional information, need to convert each char to hex
    //oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2);
    std::for_each(config.VSE_ADDITIONAL_INFORMATION.begin(), config.VSE_ADDITIONAL_INFORMATION.end(), [&oss](char c){
        oss << (int)c;
    });
    
    //calculate the payload length
    std::string payload = oss.str();
    int pl = (payload.length() / 2);

    std::stringstream fss;
    //Put it all together
    fss << "dd" << std::hex << std::setfill('0') << std::setw(2) << pl << payload;

    //Get the VSE string
    EvVSE = fss.str();

}

void ev_slacImpl::handle_reset() {
    // your code for cmd reset goes here
}

//When the command trigger matching is received, go out and try to match with a WLAN AP
//Read the beacon frames from all available networks, look for ISO 15118-8 VSEs with OID 0x70B3D53190
//Connect with the network having matching VSE and the highest signal strength.
//If an SSID override is found, then this SSID is simply connected to.
bool ev_slacImpl::handle_trigger_matching() {
    //While it is possible to use a shell command, iw dev wlan0 scan dump -u, it might be nicer to use the tins library libtins
    //sudo apt install libtins-dev
    //Unfortunately, tins needs monitor mode which is not allowed in the Raspberry Pi OS, but can be added with nexmon. Seems like a PITA, so
    //let's try to use iw dev scan instead. Another issue is that iw scan requires superuser mode. But so does tins.
    FILE *fp;
    int status;
    char nextline[1024];
    std::string nl;
    float maxsignal = -200.00;
	std::string maxbssid = "NOTFOUND";
	std::string maxssid = "NOTFOUND";

    //If the config has an ssid override, don't bother searching for the best ssid using VSE and signal strength.
    if(config.wpa_ssid_override.empty()){

        std::string iwcmd = "sudo iw dev " + config.device + " scan -u";
        fp = popen(iwcmd.c_str(), "r");
        if (fp == NULL){
            //error
            std::string errstr = fmt::format("Couldn't run iw on device {} for WiFi communication. Error: {}", config.device, strerror(errno));
            EVLOG_error << errstr;
            raise_error(error_factory->create_error("generic/CommunicationFault", "", errstr));
            return false;
        }

        std::string tssid;
        float tsignal;
        while(fgets(nextline, 1024, fp)){
            nl = nextline;
            std::size_t loc = nl.find("signal");
            //Look for the signal strength, SSID, and Vendor specific
            if(nl.find("signal") == nl.npos){
            }
            else{
                std::string ts = nl.substr(nl.find_first_of("-"),5);
                tsignal = std::stof(ts);
            }
            
            if(nl.find("SSID:") == nl.npos){
            }
            else{
                tssid = nl.substr(7, nl.npos);
                //remove the /n
                tssid = tssid.substr(0, tssid.length() - 1);
            }

            if(nl.find("Vendor specific: OUI 70:b3:d5") == nl.npos){
                //Means that the ap doesn't have a 15118-8 VSE
            }
            else{
                std::string VSEData = nl.substr(37, nl.npos);
                //Save this SSID if the signal strength is higher than the previously seen highest.
                //This seems not such a wonderful idea, it might be better to report all the ISO 15118-8 ssids along with BSSID and signal strength
                //that match our ETT back to the driver (human or not), who can then select the preferred charger AP

                //Get the ETT from the VSE and see if there is at least a match at the Energy Transfer Type
                std::string tetts = VSEData.substr(10,2);
                unsigned char tett = std::stoi(tetts, 0, 16);
                unsigned char tres = tett & ETT;
                if(0 == tres){
                    continue;                
                }

                //Now look for other interesting information that might help select an AP:
                //Country code. 
                std::string EVSECountryCode = UTF8ToString(VSEData.substr(12, 6));
                //Operator ID
                std::string EVSEOperatorID = UTF8ToString(VSEData.substr(18, 9));
                //Charging Site ID   -1 means not used, and is not utf8
                int EVSESiteID = std::stoi(UTF8ToString(VSEData.substr(27, 15)));

                //Then, optional information such as connectors, WPT information. Nothing useful for ACDs unfortunately. However, 
                //it can be assumed if a WLAN ACD is advertised by a site in the ETT, then it must be the ACDP type (inverted pantograph)
                // ACDP is only DC, and only unidirectional (not bidirectional).
                // There is a lot of info concerning WPT etc. The rest of the VSE will be put into a string
                std::string EVSEAdditionalInformation = UTF8ToString(VSEData.substr(42, VSEData.npos));

                //Though, unfortunately, there is no place to put this data in the slac interface as a var
                //Another way to approach it would be to let the user specify to use a particular countrycode, operatorid, siteid and additional information
                //in config variables instead, and use that information here to decide on a particular AP.
                //Add CountryCode, EVSEOperatorID, EVSESiteID, EVSEAdditionalInformation and allow the user to specify a match for each.
                //If any AP that doesn't have an exact match with the required info is completely rejected. Note that the additional information
                //is a string, so it can be a substring match.

                if(config.enable_wpa_logging){
                    //Log the VSE information
                    std::string logstr = fmt::format("Found VSE on SSID: {}, CountryCode: {}, OperatorID: {}, SiteID: {}, AdditionalInformation: {}", tssid, EVSECountryCode, EVSEOperatorID, EVSESiteID, EVSEAdditionalInformation);
                    EVLOG_info << logstr;
                }

                //Check if the AP matches the required information in config. If not, continue to the next AP.
                if(config.VSECountryCodeMatch != ""){
                    if(config.VSECountryCodeMatch != EVSECountryCode){
                        std::string logstr = fmt::format("VSE CountryCode mismatch on SSID: {}, config.VSECountryCodeMatch: {}, VSECountryCode: {}", tssid, config.VSECountryCodeMatch, EVSECountryCode);
                        EVLOG_info << logstr;
                        continue;
                    }
                }

                if(config.VSEOperatorIDMatch != ""){
                    if(config.VSEOperatorIDMatch != EVSEOperatorID){
                        std::string logstr = fmt::format("VSE OperatorID mismatch on SSID: {}, config.VSEOperatorIDMatch: {}, VSEOperatorID: {}", tssid, config.VSEOperatorIDMatch, EVSEOperatorID);
                        EVLOG_info << logstr;
                        continue;
                    }
                }

                if(config.VSEChargingSiteIDMatch != -1){
                    if(config.VSEChargingSiteIDMatch != EVSESiteID){
                        std::string logstr = fmt::format("VSE ChargingSiteID mismatch on SSID: {}, config.VSEChargingSiteIDMatch: {}, VSEChargingSiteID: {}", tssid, config.VSEChargingSiteIDMatch, EVSESiteID);
                        EVLOG_info << logstr;
                        continue;
                    }
                }

                if(config.VSEAdditionalInformationMatch != ""){
                    if(EVSEAdditionalInformation.find(config.VSEAdditionalInformationMatch) == std::string::npos){
                        std::string logstr = fmt::format("VSE AdditionalInformation mismatch on SSID: {}, config.VSEAdditionalInformationMatch: {}, VSEAdditionalInformation: {}", tssid, config.VSEAdditionalInformationMatch, EVSESiteID);
                        EVLOG_info << logstr;
                        continue;
                    }
                }   


                if (tsignal > maxsignal){
                    maxssid = tssid;
                    maxsignal = tsignal;
                }   
            }


        }
        status = pclose(fp);
        if (status == -1){
            //error
            std::string errstr = fmt::format("Couldn't close iw on device {} for WiFi communication. Error: {}", config.device, strerror(errno));   
            EVLOG_error << errstr;
            raise_error(error_factory->create_error("generic/CommunicationFault", "", errstr));
            return false;
        }
    }

    //No valid WiFi network found, and no override.
    if(config.wpa_ssid_override.empty() && maxssid == "NOTFOUND")
    {
        //No network to try to connect to. Not really an error, but a warning that we couldn't find a network to connect to.
        std::string errstr = fmt::format("Couldn't find suitable network for WiFi communication.");   
        EVLOG_error << errstr;
        publish_dlink_ready(false);
        publish_state("UNMATCHED");
        return false;
    }
    
    //Now, if we have found an AP with matching VSE, we need to connect to it.
    //It would be nice to at least have a password to use to connect, or it is possible to use an open WLAN, though not recommended.
    //First disconnect if we are connected. Remove all the WLAN networks (note this is ephemeral, it doesn't remove them from the wpa_supplicant.conf)
    //Make sure the wpa_ctrl is working using PING
    const char* thecmd  = "LIST_NETWORKS";
    std::stringstream ss(wpa_ctrl_request2(ctrl_conn, thecmd));
    std::string networkid;
    std::string networkssid;
    std::string networkbssid;
    std::string networkstatus;
    std::string fullcmd;
    std::string reply;

    //Read the first line and throw it away
    char firstline[1024];
    ss.getline(firstline, sizeof(firstline));

    while(ss.eof() == false){
        //Network ID
        ss >> networkid;
        if(networkid.length() == 0) break;
        ss >> networkssid;
        ss >> networkbssid;
        ss >> networkstatus;

        //remove the network
        fullcmd = "REMOVE_NETWORK " + networkid;
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    }
  

    //Add the VSE, even though we are not connected, they should be sent when the interface connects with the AP
    //First remove the element so it doesn't get duplicated. I'm not sure why to use 13 and 14...
    fullcmd = "VENDOR_ELEM_REMOVE 13 " + EvVSE;
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    fullcmd = "VENDOR_ELEM_REMOVE 14 " + EvVSE;
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    //Do a scan (not sure why)
    fullcmd = "SCAN";
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    //Add the VSE
    fullcmd = "VENDOR_ELEM_ADD 13 " + EvVSE;
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    fullcmd = "VENDOR_ELEM_ADD 14 " + EvVSE;
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

    //Now, add the network we found above, and enable it.
    fullcmd = "ADD_NETWORK";
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    //NetworkID should be 0, but let's get it from the reply
    networkid = reply.substr(0,1);
    std::string cssid = "";
    //Set up the network
    if(config.wpa_ssid_override.empty()){
        fullcmd = "SET_NETWORK " + networkid + " ssid \"" + maxssid + "\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
        cssid = maxssid;
    }
    else{ //use the override ssid
        fullcmd = "SET_NETWORK " + networkid + " ssid \"" + config.wpa_ssid_override + "\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
        cssid = config.wpa_ssid_override;
    }

    //At this point, we need to setup the credentials that will be used for WPA2-Personal or WPA2-Enterprise
    if(config.WPA_TYPE == "WPA-Enterprise-EAP-TLS"){
        const auto default_cert_path = mod->info.paths.etc / "certs";
        const auto cert_path = get_cert_path(default_cert_path, config.certificate_path);
        //Set up the EAP-TLS wpa options
        fullcmd = "SET_NETWORK "  + networkid + " key_mgmt " + "WPA-EAP";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " pairwise " + "CCMP TKIP";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " group " + "CCMP TKIP";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " eap " + "TLS";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " identity \"" + config.eap_tls_identity + "\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " ca_cert \"" + cert_path.string() + "/ca.pem\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " client_cert \"" + cert_path.string() + "/user.pem\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " private_key \"" + cert_path.string() + "/user.prv\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        fullcmd = "SET_NETWORK "  + networkid + " private_key_passwd \"" + config.private_key_password + "\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    }
    else if(config.WPA_TYPE == "WPA-Personal"){
        fullcmd = "SET_NETWORK "  + networkid + " key_mgmt " + "WPA-PSK";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

        //psk 
        fullcmd = "SET_NETWORK "  + networkid + " psk \"" + config.wpa_psk_passphrase + "\"";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
    }
    
    //Start up the network by selecting it.
    fullcmd = "SELECT_NETWORK "  + networkid;
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

    fullcmd = "SCAN";
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

    fullcmd = "LIST_NETWORKS";
    reply = wpa_ctrl_request2(ctrl_conn, fullcmd);

    //We probably need to verify that the connection is actually made, by waiting for it to connect with a callback from libwpa_client.
    //Wait for status wpa_state=COMPLETED, time out after 5 tries (seconds)
    std::size_t found;
    int i;
    for(i = 0; i < 5; i++){
        sleep(1);
        fullcmd = "STATUS";
        reply = wpa_ctrl_request2(ctrl_conn, fullcmd);
        found = reply.find("wpa_state=COMPLETED");
        if (found != reply.npos) break;
    }
    if(i == 5){
        //Connect failed.
        publish_dlink_ready(false);
        publish_state("UNMATCHED");
            std::string errstr = fmt::format("Couldn't connect to wlan network {}. Error: {}", cssid, strerror(errno));   
            EVLOG_error << errstr;
            raise_error(error_factory->create_error("generic/CommunicationFault", "", errstr));        
        return false;
    }

    //Tell Everest that the connection is made and charging can start.
    publish_dlink_ready(true);
    publish_state("MATCHED");            
    std::string errstr = fmt::format("Connected to wlan network: {}.", cssid);   
    EVLOG_info << errstr;
    return true;
}

//Assumes an input of this type:  31 90 01 and converts to the corresponding UTF8 string
std::string ev_slacImpl::UTF8ToString(std::string CodedString){

    std::string retstr;
    int loops = CodedString.length() / 3;
    for (int i = 0; i < loops; i++){
        std::string RawCode = CodedString.substr(i * 3 + 1, 2);
        char cvt =  std::stoi(RawCode, 0, 16);
        retstr.append(1,cvt);
    }
    return retstr;
}
//Helper function for wpa_ctrl_request
std::string ev_slacImpl::wpa_ctrl_request2(struct wpa_ctrl *ctrl, std::string cmd){
    char reply[4096];
    memset(reply,0,4096);
    size_t reply_len = sizeof(reply);

    int rc = wpa_ctrl_request(ctrl, cmd.c_str(), cmd.length(), reply, &reply_len, NULL );

    //get the reply and return it.
    return reply;
}
		     
		     


} // namespace main
} // namespace module
