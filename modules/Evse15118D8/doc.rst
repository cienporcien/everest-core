*******************************************
Evse15118D8
*******************************************
This Everest module implements low level communication setup and state monitoring/control of 
an ISO15118-8 WLAN connection on the EVSE side. Since EVerest is currently written to support PLC communication,
and SLAC, this module mimics (and implements) the SLAC interface similar to the EvseSlac module.

How it works:

On bootup, the EVSE WLAN is set up and starts broadcasting as specified in ISO 15118-8, including the VSEs
    - In run() in evse_slacImpl.cpp after Init is finished
The EV WLAN requests connection
    - This generates an event that starts the connection process 
The WLAN network is joined if the VSEs sent from the vehicle are recognized by the charger to be compatible and
the credentials sent by the vehicle are also accepted.
    - The event handler takes care of joining the network
    - When the join is successful, mqtt message dlink_ready=true is sent, also state=MATCHED is sent
At this point, the EV and EVSE are connected, similarly to when PLC SLAC is completed and PLC communication is connected.
Once the network is joined, communication proceeds as in the evlibiso15118 module and the ISO 15118-20 wireless SDP process is initiated

Note that WLAN works differently than PLC, the wireless SDP figures out the pairing of vehicle and charger, so even though state=MATCHED,
they are still not actually paired.


:ref:`Link <everest_modules_Evse15118D8>` to the module's reference.
Implementation of SLAC data link negotiation according to ISO15118-3.
