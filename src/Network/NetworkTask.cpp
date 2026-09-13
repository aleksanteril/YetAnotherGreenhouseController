#include "NetworkTask.h"
#include "ThingSpeakClient.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <cstring>
#include <cstdlib>

static bool parse_setpoint_command(const char *cmd, float *out_setpoint) {
    if (!cmd || !out_setpoint) {
        return false;
    }

    float val;
    char extra;
    if (sscanf(cmd, "SETPOINT %f %c", &val, &extra) != 1) { // Returns 1 if only a value is found after SETPOINT
        return false;
    }

    // Check the valid range 0 - 1500 ppm
    if (val < 0) {
        val = 0;
    }
    if (val > CO2_SETPOINT_MAX) {
        val = CO2_SETPOINT_MAX;
    }

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

    printf("[Network] starting. SSID=%s server=%s interval=%dms\n", cfg.ssid, p->server_ip, cfg.send_interval_ms);

    ThingSpeakClient client(cfg, p->server_ip, 80);
    TickType_t last_send = xTaskGetTickCount() - pdMS_TO_TICKS(cfg.send_interval_ms);

    while (true) {
        TickType_t now = xTaskGetTickCount();

        if (now - last_send >= pdMS_TO_TICKS(cfg.send_interval_ms)) {
            last_send = now;

            GreenhouseData data = shared->get();

            if (data.valid) {
                // Push measurements to the cloud.
                bool ok = client.send_update(data);
                printf("[Network] update %s: CO2=%.0f RH=%.1f T=%.1f fan=%.0f%% sp=%.0f\n",
                       ok ? "OK" : "FAIL", data.co2_ppm, data.humidity, data.temperature, data.fan_percent, data.setpoint_ppm);
            } else {
                printf("[Network] no valid measurement yet, skipping update\n");
            }

            // Check TalkBack for a remote setpoint command.
            char cmd[64];
            if (client.poll_talkback(cmd, sizeof(cmd))) {
                printf("[Network] TalkBack command: \"%s\"\n", cmd);

                float new_sp;
                if (parse_setpoint_command(cmd, &new_sp)) {
                    shared->set_setpoint(new_sp);
                    printf("[Network] remote setpoint applied: %.0f ppm\n", new_sp);
                    // todo setpoint to eeprom here
                } else {
                    printf("[Network] invalid command\n");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
