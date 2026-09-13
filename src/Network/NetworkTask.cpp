#include "NetworkTask.h"
#include "ThingSpeakClient.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// Try to parse a TalkBack command string. Currently supports:
//   "SETPOINT <ppm>"   e.g. "SETPOINT 800"
// Returns true and writes the (clamped) value to *out_setpoint on success.
static bool parse_setpoint_command(const char *cmd, float *out_setpoint) {
    if (!cmd || !out_setpoint) return false;

    // Case-insensitive match of the "SETPOINT" keyword.
    const char *kw = "SETPOINT";
    size_t kwlen = std::strlen(kw);
    if (strncasecmp(cmd, kw, kwlen) != 0) return false;

    const char *p = cmd + kwlen;
    while (*p == ' ' || *p == '\t' || *p == '=' || *p == ':') p++;

    char *end = nullptr;
    float val = std::strtof(p, &end);
    if (end == p) return false; // no number found

    // Clamp to the valid range [0, 1500] ppm per the specification.
    if (val < 0.0f) val = 0.0f;
    if (val > CO2_SETPOINT_MAX) val = CO2_SETPOINT_MAX;

    *out_setpoint = val;
    return true;
}

void network_task(void *param) {
    auto *p = static_cast<NetworkTaskParams *>(param);
    if (!p || !p->shared) {
        printf("[Network] invalid params, task exiting\n");
        vTaskDelete(nullptr);
        return;
    }

    SharedData *shared = p->shared;
    const NetworkSettings &cfg = p->settings;

    printf("[Network] starting. SSID=%s server=%s interval=%dms\n",
           cfg.ssid, p->server_ip, cfg.send_interval_ms);

    // The ThingSpeakClient constructs an IPStack per request, which brings up
    // WiFi. We keep the client for the lifetime of the task.
    ThingSpeakClient client(cfg, p->server_ip, 80);

    TickType_t last_send = xTaskGetTickCount() - pdMS_TO_TICKS(cfg.send_interval_ms);

    for (;;) {
        TickType_t now = xTaskGetTickCount();

        if ((now - last_send) >= pdMS_TO_TICKS(cfg.send_interval_ms)) {
            last_send = now;

            GreenhouseData data = shared->get();

            if (data.valid) {
                // 1. Push measurements to the cloud.
                bool ok = client.send_update(data);
                printf("[Network] update %s: CO2=%.0f RH=%.1f T=%.1f fan=%.0f%% sp=%.0f\n",
                       ok ? "OK" : "FAIL",
                       data.co2_ppm, data.humidity_rh, data.temperature,
                       data.fan_percent, data.setpoint_ppm);
            } else {
                printf("[Network] no valid measurement yet, skipping update\n");
            }

            // 2. Check TalkBack for a remote setpoint command.
            char cmd[64];
            if (client.poll_talkback(cmd, sizeof(cmd))) {
                printf("[Network] TalkBack command: \"%s\"\n", cmd);

                float new_sp;
                if (parse_setpoint_command(cmd, &new_sp)) {
                    shared->set_setpoint(new_sp);
                    printf("[Network] remote setpoint applied: %.0f ppm\n", new_sp);
                    // NOTE: when EEPROM settings are added, persist here too.
                } else {
                    printf("[Network] unrecognised command, ignored\n");
                }
            }
        }

        // Sleep in small slices so the task stays responsive without busy-waiting.
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
