#ifndef CO2_THINGSPEAK_PACKET_H
#define CO2_THINGSPEAK_PACKET_H

#include "pico/types.h"

struct ThingSpeakPacket {
    uint16_t co2_ppm      = 0;      // field1 - measured CO2 level
    double humidity     = 0;      // field2 - relative humidity (%)
    double temperature  = 0;      // field3 - temperature (C)
    double fan_percent  = 0;      // field4 - ventilation fan speed 0-100%
    uint16_t setpoint_ppm = 0;      // field5 - CO2 set point
};

#endif // CO2_THINGSPEAK_PACKET_H
