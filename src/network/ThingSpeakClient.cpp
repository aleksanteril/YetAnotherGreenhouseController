#include "ThingSpeakClient.h"

#include "pico/cyw43_arch.h"

#include <cstdio>
#include <string.h>
#include "constants.hpp"

// Response buffer for ThingSpeak's replies and headers.
#define RESP_BUF_SIZE  1024

ThingSpeakClient::ThingSpeakClient
(TLSClient &ipstack, ThingSpeakConfig &settings, const char *server, const int port, EEPROM& e)
    : ip(ipstack), cfg(settings), server(server), port(port), e(e)
{
}

int ThingSpeakClient::exchange(char *request, char *resp_buf, int resp_buf_size) 
{
    // write request to our TLS object
    int wlen = (int)strlen(request);
    int wr = ip.write(request, wlen);

    int rc = ip.connect(server, port, 20000);
    if (rc != 0) {
        return -1;
    }
    
    // read our resp buffer from TLS object
    int total = ip.read(resp_buf, resp_buf_size - 1);
    if (total < 0) {
        total = 0;
    }
    resp_buf[total] = '\0';
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
    char request[512];
    {
        char body[256];
        const int body_len = snprintf(
                body, sizeof(body),
                "field1=%d" 
                "&field2=%.1lf" 
                "&field3=%.1lf"                      
                "&field4=%d" 
                "&field5=%d"
                "&api_key=%s" 
                "&talkback_key=%s",
                data.co2_ppm,
                data.humidity,
                data.temperature,
                data.fan_percent,
                data.setpoint_ppm,
                cfg.api_key,
                cfg.talkback_key);

        const int req_len = snprintf(
                request, sizeof(request),
                "POST /update.json HTTP/1.1\r\n"
                "Host: api.thingspeak.com\r\n"
                "User-Agent: PicoW\r\n"
                "Accept: */*\r\n"
                "Content-Length: %d\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "\r\n"
                "%s", body_len, body);
        
        if (req_len < 0 || req_len >= (int)sizeof(request)) {
                return false;
        }
    }
    char resp[RESP_BUF_SIZE];
    int n = exchange(request, resp, sizeof(resp));
    if (n <= 0) {
        return false;
    }
    if (http_status_code(resp) != 200) {
        return false;
    } 
    char cmd[32];
    if (parse_talkback(resp, cmd, sizeof(cmd))) {
        if(int sp; (sp = parse_setpoint(cmd)) >= 0) {
                e.save_setpoint(sp);
        }
    };
    
    // /update.json returns the new entry id in the body on success, and 200.
    return true;
}

bool ThingSpeakClient::parse_talkback(char* resp, char* out_cmd, size_t size) 
{
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
    while (*p && *p != '"' && i < size - 1) {
        out_cmd[i++] = *p++;
    }
    out_cmd[i] = '\0';

    return i > 0;
}

int
ThingSpeakClient::parse_setpoint(const char *cmd) 
{
        int val{};
        char extra{};
        if (sscanf(cmd, "SP=%d %c", &val, &extra) !=
            1) { // Returns 1 if only a value is found after SETPOINT
                return -1;
        }

        // Check the valid range 0 - 1500 ppm
        if (val < 0) {
                val = 0;
        }
        if (val > MAX_CO2_SP) {
                val = MAX_CO2_SP;
        }
        return val;
}

bool
ThingSpeakClient::reconnect() 
{
        int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (status == CYW43_LINK_UP) {
                // WiFi ok
                return true;
        }
        // Not connected
        return ip.reconnect_wifi(cfg.ssid, cfg.password) == 0;
}
