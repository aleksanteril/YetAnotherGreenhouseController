#ifndef CO2CONTROLLER_NETWORK_H
#define CO2CONTROLLER_NETWORK_H
#include "secret.h"

struct NetworkSettings {
    char ssid[64]          = "";
    char password[64]      = "";

    // ThingSpeak keys
    char api_key[24]       = "";
    char talkback_key[24]  = "";

    int send_interval_ms   = 16000;

    bool valid() const {
        return ssid[0] != '\0' && api_key[0] != '\0';
    }
};

#endif //CO2CONTROLLER_NETWORK_H