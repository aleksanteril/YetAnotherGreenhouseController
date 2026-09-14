#ifndef CO2CONTROLLER_NETWORK_H
#define CO2CONTROLLER_NETWORK_H

#define THINGSPEAK_HOST "api.thingspeak.com"

#include "../secret.h"

struct ThingSpeakConfig {
    // MAcros from secret.h

    const char* ssid          = WIFI_SSID;
    const char* password      = WIFI_PASSWORD;

    // ThingSpeak keys
    const char* api_key      = TS_API_KEY;
    int  talkback_id       = TS_TALCKBACK_ID;
    const char* talkback_key  = TS_TALCKBACK_KEY;

    const char* server_ip     = TS_SERVER_IP;

    int send_interval_ms   = 16000;

    bool valid() const {
        return ssid[0] != '\0' && api_key[0] != '\0';
    }
};

#endif //CO2CONTROLLER_NETWORK_H
