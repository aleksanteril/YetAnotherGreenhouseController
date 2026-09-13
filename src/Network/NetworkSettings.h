#ifndef CO2CONTROLLER_NETWORK_H
#define CO2CONTROLLER_NETWORK_H

#define THINGSPEAK_HOST "api.thingspeak.com"

struct NetworkSettings {
    char ssid[64]          = "";
    char password[64]      = "";

    // ThingSpeak keys
    char api_key[24]       = "371DAWENQKI6J8DD";
    int  talkback_id       = 52920;
    char talkback_key[24]  = "1WWH2NWXSM53URR5";

    int send_interval_ms   = 16000;

    bool valid() const {
        return ssid[0] != '\0' && api_key[0] != '\0';
    }
};

#endif //CO2CONTROLLER_NETWORK_H