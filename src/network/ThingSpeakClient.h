#ifndef CO2CONTROLLER_THINGSPEAK_CLIENT_H
#define CO2CONTROLLER_THINGSPEAK_CLIENT_H

#include "NetworkSettings.h"
#include "ThingSpeakPacket.h"
#include "TLSClient.h"
#include "eeprom.hpp"

class ThingSpeakClient {
public:
    ThingSpeakClient(TLSClient &ipstack, ThingSpeakConfig &settings,
                     const char *server, const int port, EEPROM& e);

    // Returns true if the server replied.
    // this now sends the fields and also clears the talkback queue by 1
    bool send_update(const ThingSpeakPacket &data);

    // reconnect to wifi
    bool reconnect();

private:
    // Perform one request/response exchange
    // Writes the response into resp_buf (NUL-terminated)
    // Returns number of bytes read, or < 0 on error
    int exchange(char *request, char *resp_buf, int resp_buf_size);
    // Extract the HTTP status code from a response
    static int http_status_code(const char *response);
    // Locate the response body
    static const char *http_body(const char *response);

    bool parse_talkback(char* resp, char* out_cmd, size_t size);
    int parse_setpoint(const char *cmd);

    TLSClient &ip;
    ThingSpeakConfig &cfg;
    const char* server;
    const int port;
    EEPROM& e;
};

#endif //CO2CONTROLLER_THINGSPEAK_CLIENT_H
