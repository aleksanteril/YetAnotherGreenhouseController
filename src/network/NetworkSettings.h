#ifndef CO2CONTROLLER_NETWORK_H
#define CO2CONTROLLER_NETWORK_H

#define THINGSPEAK_HOST "api.thingspeak.com"

#include "../secret.h"

struct ThingSpeakConfig {
    // MAcros from secret.h

    const char* ssid;          //= WIFI_SSID;
    const char* password;      //= WIFI_PASSWORD;

    // ThingSpeak keys
    const char* api_key      = TS_API_KEY;
    int  talkback_id       = TS_TALKBACK_ID;
    const char* talkback_key  = TS_TALKBACK_KEY;
};

#endif //CO2CONTROLLER_NETWORK_H
