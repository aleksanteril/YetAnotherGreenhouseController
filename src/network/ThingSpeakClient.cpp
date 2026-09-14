#include "ThingSpeakClient.h"

#include "IPStack.h"

#include <cstdio>
#include <string.h>
#include "constants.hpp"

// Response buffer for ThingSpeak's replies and headers.
#define RESP_BUF_SIZE  1024

ThingSpeakClient::ThingSpeakClient(IPStack &ipstack, ThingSpeakConfig &settings, const char *server_ip, const int port)
: ip(ipstack), cfg(settings), port(port), server_ip(server_ip) 
{
}

int ThingSpeakClient::exchange(const char *request, char *resp_buf, int resp_buf_size) {
    int rc = ip.connect(server_ip, port);
    if (rc != 0) {
        return -1;
    }

    int wlen = (int)strlen(request);
    int wr = ip.write((unsigned char *)request, wlen, 1000);
    if (wr < 0) {
        ip.disconnect();
        return -2;
    }

    int total = ip.read((unsigned char *)resp_buf, resp_buf_size - 1, 5000);
    if (total < 0) {
        total = 0;
    }
    resp_buf[total] = '\0';

    ip.disconnect();
    return total;
}

int ThingSpeakClient::http_status_code(const char *response) {
    // Expect "HTTP/1.1 200 OK\r\n"
    if (!response) {
        return -1;
    }
    const char *sp = strchr(response, ' ');
    if (!sp) {
        return -1;
    }

    return atoi(sp + 1);
}

const char *ThingSpeakClient::http_body(const char *response) {
    if (!response) {
        return nullptr;
    }

    const char *body = strstr(response, "\r\n\r\n");
    if (body) {
        return body + 4;
    }

    body = strstr(response, "\n\n");
    if (body) {
        return body + 2;
    }

    return nullptr;
}

bool ThingSpeakClient::send_update(const ThingSpeakPacket &data) {
    // field1=CO2, field2=RH, field3=Temp, field4=Fan%, field5=Setpoint
    char body[256];
    const int body_len = snprintf(
        body, sizeof(body),
        "api_key=%s"
        "&field1=%d"
        "&field2=%.1lf"
        "&field3=%.1lf"
        "&field4=%d"
        "&field5=%d",
        cfg.api_key,
        data.co2_ppm,
        data.humidity,
        data.temperature,
        data.fan_percent,
        data.setpoint_ppm);

    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        return false;
    }

    char request[512];
    const int req_len = snprintf(
        request, sizeof(request),
        "POST /update.json HTTP/1.1\r\n"
        "Host: " THINGSPEAK_HOST "\r\n"
        "Connection: close\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        body_len, body);

    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        return false;
    }

    char resp[RESP_BUF_SIZE];
    int n = exchange(request, resp, sizeof(resp));
    if (n <= 0) {
        return false;
    }

    int status = http_status_code(resp);
    // /update.json returns the new entry id in the body on success, and 200.
    return status == 200;
}

bool ThingSpeakClient::poll_talkback(char *out_cmd, int out_cmd_size) {
    if (!out_cmd || out_cmd_size <= 0) {
        return false;
    }
    out_cmd[0] = '\0';

    // Body is just talkback api_key
    char body[64];
    int body_len = std::snprintf(body, sizeof(body), "api_key=%s", cfg.talkback_key);
    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        return false;
    }

    char request[256];
    int req_len = std::snprintf(
        request, sizeof(request),
        "POST /talkbacks/%d/commands/execute.json HTTP/1.1\r\n"
        "Host: " THINGSPEAK_HOST "\r\n"
        "Connection: close\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        cfg.talkback_id, body_len, body);

    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        return false;
    }

    char resp[RESP_BUF_SIZE];
    if (exchange(request, resp, sizeof(resp)) <= 0) {
        return false;
    }

    if (http_status_code(resp) != 200) {
        return false;
    }

    const char *bdy = http_body(resp);
    if (!bdy) {
        return false;
    }

    // returns JSON such as: {"id":123,"command_string":"SETPOINT 800", ...}
    const char *key = strstr(bdy, "\"command_string\"");
    if (!key) {
        return false;
    }

    const char *colon = strchr(key, ':');
    if (!colon) {
        return false;
    }

    // Skip spaces and the opening quote.
    const char *p = colon + 1;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (*p != '"') {
        return false;
    }

    p++;

    int i = 0;
    while (*p && *p != '"' && i < out_cmd_size - 1) {
        out_cmd[i++] = *p++;
    }
    out_cmd[i] = '\0';

    return i > 0;
}

bool 
ThingSpeakClient::parse_setpoint(const char *cmd, int *out_setpoint) 
{
        if (!cmd || !out_setpoint) {
                return false;
        }

        int val{};
        char extra{};
        if (sscanf(cmd, "SETPOINT %d %c", &val, &extra) !=
            1) { // Returns 1 if only a value is found after SETPOINT
                return false;
        }

        // Check the valid range 0 - 1500 ppm
        if (val < 0) {
                val = 0;
        }
        if (val > MAX_CO2_SP) {
                val = MAX_CO2_SP;
        }

        *out_setpoint = val;
        return true;
}

bool
ThingSpeakClient::reconnect() 
{
        return ip.reconnect_wifi(cfg.ssid, cfg.password) == 0;
}

bool
ThingSpeakClient::reconnect(const char* ssid, const char* pwd)
{
        cfg.ssid = ssid;
        cfg.password = pwd;
        return ip.reconnect_wifi(ssid, pwd) == 0;
}
