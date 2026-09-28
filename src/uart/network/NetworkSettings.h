#ifndef CO2CONTROLLER_NETWORK_H
#define CO2CONTROLLER_NETWORK_H

#define THINGSPEAK_HOST "api.thingspeak.com"

#include "../secret.h"

struct NetworkSettings {
    // MAcros from secret.h

    char ssid[64]          = WIFI_SSID;
    char password[64]      = WIFI_PASSWORD;

    // ThingSpeak keys
    char api_key[24]       = TS_API_KEY;
    int  talkback_id       = TS_TALCKBACK_ID;
    char talkback_key[24]  = TS_TALCKBACK_KEY;

    char server_ip[32]     = TS_SERVER_IP;

    int send_interval_ms   = 16000;

    bool valid() const {
        return ssid[0] != '\0' && api_key[0] != '\0';
    }
};

#endif //CO2CONTROLLER_NETWORK_H