*******************************************
Ev15118D8
*******************************************

This Everest module implements low level communication setup and state monitoring/control of 
an ISO15118-8 WLAN connection on the EV side. Since EVerest is currently written to support PLC communication,
and SLAC, this module mimics (and implements) the ev_slac interface similarly to the EvSlac module.

How it works:

On bootup, the EV WLAN is set up.as specified in ISO 15118-8
    - - The module is initialized and the WLAN is set up
The vehicle arrives within range of one or more charger WLAN networks that are broadcasting ISO 15118-8 Vender Specific Elements (VSEs)
The driver initiates the WLAN network join process
    - The driver initiates the WLAN network join process by pressing a button or similar
The EV WLAN network join process is initiated
    - we use the existing cmd trigger_matching for this
The WLAN network is joined if the VSEs sent from the vehicle are recognized by the charger to be compatible and
the credentials sent by the vehicle are also accepted.
    - The trigger_matching event handler takes care of joining the network
    - When the join is successful, mqtt message dlink_ready=true is sent, also state=MATCHED is sent. if not successful, dlink_ready=false and state=FAILED is sent
At this point, the EV and EVSE are connected, similarly to when PLC SLAC is completed and PLC communication is connected.
Once the network is joined, communication proceeds as in the evlibiso15118 module and the ISO 15118-20 wireless SDP process is initiated

Note that WLAN works differently than PLC, the wireless SDP figures out the pairing of vehicle and charger, so even though state=MATCHED,
they are still not actually paired.



:ref:`Link <everest_modules_Ev15118D8>` to the module's reference.
Implementation of EV WLAN data link negotiation according to ISO15118-8.
