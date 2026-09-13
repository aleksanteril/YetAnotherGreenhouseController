#include "ThingSpeakClient.h"
#include "IPStack.h"

#include <cstdio>
#include <cstring>

// Response buffer big enough for ThingSpeak's small JSON replies plus headers.
static constexpr int RESP_BUF_SIZE = 1024;

ThingSpeakClient::ThingSpeakClient(const NetworkSettings &settings, const char *server_ip_, int port_)
                                  : cfg(settings), port(port_) {
    set_server_ip(server_ip_);
}

void ThingSpeakClient::set_server_ip(const char *ip) {
    if (ip) {
        std::strncpy(server_ip, ip, sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
    }
}

int ThingSpeakClient::exchange(const char *request, char *resp_buf, int resp_buf_size) {
    // A fresh IPStack per exchange keeps the design simple and robust: WiFi is
    // already up (IPStack ctor connects), and we open/close one TCP connection
    // per request. ThingSpeak sends "Connection: close" style short replies.
    IPStack ip(cfg.ssid, cfg.password);

    int rc = ip.connect(server_ip, port);
    if (rc != 0) {
        printf("[ThingSpeak] TCP connect to %s:%d failed rc=%d\n",
               server_ip, port, rc);
        return -1;
    }

    int wlen = (int)std::strlen(request);
    int wr = ip.write((unsigned char *)request, wlen, 1000);
    if (wr < 0) {
        printf("[ThingSpeak] write failed rc=%d\n", wr);
        ip.disconnect();
        return -2;
    }

    int total = ip.read((unsigned char *)resp_buf,
                        resp_buf_size - 1, 2000);
    if (total < 0) total = 0;
    resp_buf[total] = '\0';

    ip.disconnect();
    return total;
}

int ThingSpeakClient::http_status_code(const char *response) {
    // Expect "HTTP/1.1 200 OK\r\n"
    if (!response) return -1;
    const char *sp = std::strchr(response, ' ');
    if (!sp) return -1;
    return std::atoi(sp + 1);
}

const char *ThingSpeakClient::http_body(const char *response) {
    if (!response) return nullptr;
    const char *body = std::strstr(response, "\r\n\r\n");
    if (body) return body + 4;
    body = std::strstr(response, "\n\n");
    if (body) return body + 2;
    return nullptr;
}

bool ThingSpeakClient::send_update(const GreenhouseData &data) {
    // Build the form body first so we can set Content-Length correctly.
    // field1=CO2, field2=RH, field3=Temp, field4=Fan%, field5=Setpoint
    char body[256];
    int body_len = std::snprintf(
        body, sizeof(body),
        "api_key=%s"
        "&field1=%.0f"
        "&field2=%.1f"
        "&field3=%.1f"
        "&field4=%.0f"
        "&field5=%.0f",
        cfg.api_key,
        data.co2_ppm,
        data.humidity_rh,
        data.temperature,
        data.fan_percent,
        data.setpoint_ppm);

    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        printf("[ThingSpeak] update body overflow\n");
        return false;
    }

    char request[512];
    int req_len = std::snprintf(
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
        printf("[ThingSpeak] update request overflow\n");
        return false;
    }

    char resp[RESP_BUF_SIZE];
    int n = exchange(request, resp, sizeof(resp));
    if (n <= 0) return false;

    int status = http_status_code(resp);
    // /update.json returns the new entry id in the body on success, and 200.
    printf("[ThingSpeak] update status=%d\n", status);
    return status == 200;
}

bool ThingSpeakClient::poll_talkback(char *out_cmd, int out_cmd_size) {
    if (!out_cmd || out_cmd_size <= 0) return false;
    out_cmd[0] = '\0';

    // Body is just the talkback api_key.
    char body[64];
    int body_len = std::snprintf(body, sizeof(body),
                                 "api_key=%s", cfg.talkback_key);
    if (body_len < 0 || body_len >= (int)sizeof(body)) return false;

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

    if (req_len < 0 || req_len >= (int)sizeof(request)) return false;

    char resp[RESP_BUF_SIZE];
    int n = exchange(request, resp, sizeof(resp));
    if (n <= 0) return false;

    if (http_status_code(resp) != 200) return false;

    const char *bdy = http_body(resp);
    if (!bdy) return false;

    // Empty queue returns a body of "0" (or nearly empty). A queued command
    // returns JSON such as: {"id":123,"command_string":"SETPOINT 800", ...}
    const char *key = std::strstr(bdy, "\"command_string\"");
    if (!key) return false;

    const char *colon = std::strchr(key, ':');
    if (!colon) return false;

    // Skip spaces and the opening quote.
    const char *p = colon + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;

    int i = 0;
    while (*p && *p != '"' && i < out_cmd_size - 1) {
        out_cmd[i++] = *p++;
    }
    out_cmd[i] = '\0';

    return i > 0;
}
