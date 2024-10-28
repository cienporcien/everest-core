// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "pairing_and_positioningImpl.hpp"

//Uses libserial
#include <libserial/SerialStream.h>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <fmt/core.h>
#include <thread>

constexpr const char* const SERIAL_PORT_1 = "/dev/ttyUSB0" ;


namespace module {
namespace main {

void pairing_and_positioningImpl::init() {
    using namespace LibSerial ;

    try
    {
        // Open the Serial Port at the desired hardware port.
        serial_stream.Open(config.serial_port) ;
    }
    catch (const OpenFailed&)
    {
        
        std::string errstr = fmt::format("Module \"UWBPPD\" could not be initialized! The serial port: {} did not open correctly. Error: {}", config.serial_port, strerror(errno));
        EVLOG_error << errstr;
        //raise_error(error_factory->create_error("generic/CommunicationFault", "", errstr));
    }

    // Set the baud rate of the serial port. TODO fix
    serial_stream.SetBaudRate(BaudRate::BAUD_3000000) ;

    // Set the number of data bits.
    serial_stream.SetCharacterSize(CharacterSize::CHAR_SIZE_8) ;

    // Turn off hardware flow control.
    serial_stream.SetFlowControl(FlowControl::FLOW_CONTROL_NONE) ;

    // Disable parity.
    serial_stream.SetParity(Parity::PARITY_NONE) ;

    // Set the number of stop bits.
    serial_stream.SetStopBits(StopBits::STOP_BITS_1) ;


}

void pairing_and_positioningImpl::ready() {
        std::thread t([this] {
            serial_stream.FlushInputBuffer();
            while (true) {
                read_UWB();
                sleep(1);
            }
        });
        t.detach();
}


#define PI 3.14159265

void pairing_and_positioningImpl::read_UWB() {
    // Variable to store data coming from the serial port.
    std::string data_line;
    if (serial_stream.IsDataAvailable()) {

        // Read three lines at a time
        for (int i = 0; i < 3; i++) {
            // Read a single byte of data from the serial port.
            // serial_stream.get(data_byte) ;
            
            //std::getline appears to block and wait forever...
            std::getline(serial_stream, data_line);

            // Extract just the item and the value
            auto subdata_line = data_line.substr(data_line.find_first_of('.') + 1);

            auto reading_type = subdata_line.substr(0, subdata_line.find_first_of(' '));
            auto reading = subdata_line.substr(subdata_line.find_first_of(':') + 1);

            if (reading_type == "distance") {
                distance = std::stod(reading);
            } else if (reading_type == "aoa_azimuth") {
                aoa_azimuth = std::stod(reading);
            } else if (reading_type == "aoa_elevation") {
                aoa_elevation = std::stod(reading);

                types::pairing_and_positioning::Position tp;
                // Calculate the relative position Convert from spherical to cartesian
                // x = radius * cos(phi) * sin(theta)
                // y = radius * sin(phi) * sin(theta)
                // z = radius * cos(theta)
                tp.position_X = distance * cos(aoa_azimuth * PI / 180) * sin(aoa_elevation * PI / 180);
                tp.position_Y = distance * sin(aoa_azimuth * PI / 180) * sin(aoa_elevation * PI / 180);
                tp.position_Z = distance * cos(aoa_azimuth * PI / 180);

                // Publish the position.
                publish_PositionMeasurement(tp);

            } else {
                // error, flag with negative distance 1 meter
                distance = -100.0;
            }
        }
    } else {
        // Send a negative distance (impossible), since no data
        types::pairing_and_positioning::Position tp;

        tp.position_Z = -100.0;
        tp.position_X = 0.0;
        tp.position_Y = 0.0;

        // Publish the position.
        publish_PositionMeasurement(tp);
    }
}

} // namespace main
} // namespace module
