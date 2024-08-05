// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "ev_slacImpl.hpp"
#include "wpa_ctrl.h" //lives in /usr/include after installing libwpa-client-dev (sudo apt install libwpa-client-dev)
// #include "tins/tins.h" //libtins for packet sniffing to get the VSEs from the APs

//Nice way to know when to start running.
static std::promise<void> module_ready;

namespace module {
namespace main {

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
    struct wpa_ctrl *ctrl_conn = wpa_ctrl_open(config.device.c_str());
	if (ctrl_conn == NULL){
        //error
        return;
    }

    //Make sure the wpa_ctrl is working using PING
    const char* thecmd  = "PING";
    char reply[4000];
    size_t reply_len = 3999;
    int rc = wpa_ctrl_request(ctrl_conn, thecmd, sizeof(thecmd), reply, &reply_len, NULL );
	std::string reply_str = reply;
    if(reply_str.substr(0, 4) != "PONG"){
        //error, close and leave
        wpa_ctrl_close(ctrl_conn);
        return;
    }

    //Form the EV side VSE
    std::string EvVSE;
    //Element ID
    EvVSE.append(1, 0xDD);
    //Payload Length (0x7 to 0xFF) - change later once we know the length
    EvVSE.append(1, 0x07);
    //Unique ISO Organization ID
    unsigned char cp[5] = {0x70, 0xB3, 0xD5, 0x31, 0x90};
    EvVSE.append(reinterpret_cast<char *>(cp), 5);
    //Element ID (EV VSE)
    EvVSE.append(1, 0x02);
    //Form a byte for the Energy Transfer Type (ETT)
    unsigned char ETT = 0;
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
    EvVSE.append(1, ETT);

    //The additional information
    EvVSE.append(config.VSE_ADDITIONAL_INFORMATION);

    //Write the length into the second character
    EvVSE[1] = EvVSE.length();

    //Read the beacon frames from all available networks, look for ISO 15118-8 VSEs with OID 0x70B3D53190
    //While it is possible to use a shell command, iw dev wlan0 scan dump -u, it might be nicer to use the tins library libtins
    //sudo apt install libtins-dev
    //Unfortunately, tins needs monitor mode which is not allowed in the Raspberry Pi OS, but can be added with nexmon. Seems like a PITA, so
    //let's try to use iw dev instead. Another issue is that iw scan requires superuser mode. But so does tins.
    FILE *fp;
    int status;
    char nextline[1024];
    std::string nl;
    float maxsignal = -200.00;
	std::string maxbssid = "NOTFOUND";
	std::string maxssid = "NOTFOUND";

    fp = popen("sudo iw dev wlan0 scan -u", "r");
    if (fp == NULL){
        //error
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
        }

        if(nl.find("Vendor specific: OUI 70:b3:d5") == nl.npos){
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
            if(0 != tres){
                maxssid = tssid;                
            }

            //Now look for other interesting information that might help select an AP:
            //Country code. 
            std::string EVSECountryCode = UTF8ToString(VSEData.substr(12, 6));
            //Operator ID
            std::string EVSEOperatorID = UTF8ToString(VSEData.substr(18, 9));
            //Charging Site ID
            std::string EVSESiteID = UTF8ToString(VSEData.substr(27, 15));

            //Then, optional information such as connectors, WPT information. Nothing useful for ACDs unfortunately. However, 
            //it can be assumed if a WLAN ACD is advertised by a site in the ETT, then it must be the ACDP type (inverted pantograph)
            // ACDP is only DC, and only unidirectional (not bidirectional).
            // There is a lot of info concerning WPT etc. The rest of the VSE will be put into a string
            std::string EVSEAdditionalInformation = UTF8ToString(VSEData.substr(42, VSEData.npos));

            if (tsignal > maxsignal){
                maxssid = tssid;
            }
        }


    }
    status = pclose(fp);
    if (status == -1){
        //error
    }


    //Close the WLAN interface
    wpa_ctrl_close(ctrl_conn);


}

void ev_slacImpl::handle_reset() {
    // your code for cmd reset goes here
}

bool ev_slacImpl::handle_trigger_matching() {
    // your code for cmd trigger_matching goes here
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


} // namespace main
} // namespace module
