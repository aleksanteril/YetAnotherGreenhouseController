//
// NetworkTask.h - FreeRTOS task that links the greenhouse controller to the
// ThingSpeak cloud service.
//
// The task:
//   1. Sends the five required fields to ThingSpeak on a fixed interval
//      (>= 15 s as required by the free tier).
//   2. Polls the TalkBack queue for remote commands and applies a new CO2
//      setpoint when a "SETPOINT <ppm>" command is received.
//
// It is deliberately decoupled from the controller: it only touches the
// SharedData object, so WiFi problems never stall the control loop.
//

#ifndef CO2_NETWORK_TASK_H
#define CO2_NETWORK_TASK_H

#include "SharedData.h"
#include "NetworkSettings.h"

// Parameter block passed to the task via xTaskCreate.
struct NetworkTaskParams {
    SharedData      *shared;    // shared measurements + setpoint
    NetworkSettings  settings;  // connection parameters (copied by value)
    const char      *server_ip; // dotted-decimal IP for api.thingspeak.com
};

// Upper limit for the CO2 setpoint from the specification.
static constexpr float CO2_SETPOINT_MAX = 1500.0f;

// FreeRTOS task entry point. `param` must point to a NetworkTaskParams that
// stays valid for the lifetime of the task (e.g. a static/global).
void network_task(void *param);

#endif // CO2_NETWORK_TASK_H
