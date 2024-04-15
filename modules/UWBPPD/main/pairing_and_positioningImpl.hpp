// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef MAIN_PAIRING_AND_POSITIONING_IMPL_HPP
#define MAIN_PAIRING_AND_POSITIONING_IMPL_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 3
//

#include <generated/interfaces/pairing_and_positioning/Implementation.hpp>

#include "../UWBPPD.hpp"

// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1
// insert your custom include headers here
#include <libserial/SerialStream.h>
// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1

namespace module {
namespace main {

struct Conf {
    double baud_rate;
    std::string serial_port;
};

class pairing_and_positioningImpl : public pairing_and_positioningImplBase {
public:
    pairing_and_positioningImpl() = delete;
    pairing_and_positioningImpl(Everest::ModuleAdapter* ev, const Everest::PtrContainer<UWBPPD>& mod, Conf& config) :
        pairing_and_positioningImplBase(ev, "main"), mod(mod), config(config){};

    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1
    // insert your public definitions here
    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1

protected:

    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1
    // insert your protected definitions here
    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1

private:
    const Everest::PtrContainer<UWBPPD>& mod;
    const Conf& config;

    virtual void init() override;
    virtual void ready() override;

    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
    // insert your private definitions here
    void read_UWB();
    // Instantiate a SerialStream object.
    LibSerial::SerialStream serial_stream;
    types::pairing_and_positioning::Position uwb_position;
    double distance = 0.0;
    double aoa_azimuth = 0.0;
    double aoa_elevation = 0.0;
    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
};

// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1
// insert other definitions here
// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1

} // namespace main
} // namespace module

#endif // MAIN_PAIRING_AND_POSITIONING_IMPL_HPP
