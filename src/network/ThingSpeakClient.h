#ifndef CO2CONTROLLER_THINGSPEAK_CLIENT_H
#define CO2CONTROLLER_THINGSPEAK_CLIENT_H

#include "NetworkSettings.h"
#include "SharedData.h"
#include "IPStack.h"

class ThingSpeakClient {
public:
    ThingSpeakClient(IPStack &ipstack, ThingSpeakConfig &settings,
                     const char *server_ip, int port = 80);

    // Returns true if the server replied
    bool send_update(const GreenhouseData &data);

    // On success, copies the command string into out_cmd and returns true
    // Returns false if there is no command, on network error, or on parse failure
    bool poll_talkback(char *out_cmd, int out_cmd_size);

    void set_server_ip(const char *ip);

private:
    // Perform one request/response exchange
    // Writes the response into resp_buf (NUL-terminated)
    // Returns number of bytes read, or < 0 on error
    int exchange(const char *request, char *resp_buf, int resp_buf_size);

    // Extract the HTTP status code from a response
    static int http_status_code(const char *response);

    // Locate the response body
    static const char *http_body(const char *response);

    IPStack &ip;
    const ThingSpeakConfig &cfg;
    char server_ip[32];
    int  port;
};

#endif //CO2CONTROLLER_THINGSPEAK_CLIENT_H
