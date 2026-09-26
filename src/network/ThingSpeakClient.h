#ifndef CO2CONTROLLER_THINGSPEAK_CLIENT_H
#define CO2CONTROLLER_THINGSPEAK_CLIENT_H

#include "NetworkSettings.h"
#include "ThingSpeakPacket.h"
#include "TlsClient.h"

class ThingSpeakClient {
public:
    ThingSpeakClient(TlsClient &ipstack, ThingSpeakConfig &settings,
                     const char *server_ip, int port = 443);

    // Returns true if the server replied
    bool send_update(const ThingSpeakPacket &data);

    // On success, copies the command string into out_cmd and returns true
    // Returns false if there is no command, on network error, or on parse failure
    bool poll_talkback(char *out_cmd, int out_cmd_size);

    void set_server_ip(const char *ip);

    bool parse_setpoint(const char *cmd, int *out_setpoint);

    // reconnect to wifi
    bool reconnect();

private:
    // Perform one request/response exchange
    // Writes the response into resp_buf (NUL-terminated)
    // Returns number of bytes read, or < 0 on error
    int exchange(const char *request, char *resp_buf, int resp_buf_size);

    // Extract the HTTP status code from a response
    static int http_status_code(const char *response);

    // Locate the response body
    static const char *http_body(const char *response);

    TlsClient &ip;
    ThingSpeakConfig &cfg;
    const char* server_ip;
    int port;
};

#endif //CO2CONTROLLER_THINGSPEAK_CLIENT_H
