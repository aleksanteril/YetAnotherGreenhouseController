#ifndef CO2_NETWORK_TASK_H
#define CO2_NETWORK_TASK_H

#include "SharedData.h"
#include "NetworkSettings.h"

// Upper limit for the CO2 setpoint from the specification.
#define CO2_SETPOINT_MAX 1500


struct NetworkTaskParams {
    SharedData      *shared;    // shared measurements + setpoint
    NetworkSettings  settings;  // connection parameters (copied by value)
    const char      *server_ip; // dotted-decimal IP for api.thingspeak.com
};

void network_task(void *param);

#endif // CO2_NETWORK_TASK_H
