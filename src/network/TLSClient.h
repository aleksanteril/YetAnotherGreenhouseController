
#ifndef CO2CONTROLLER_TLS_CLIENT_H
#define CO2CONTROLLER_TLS_CLIENT_H

#include "FreeRTOS.h"
#include "task.h"

#include <cstdint>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"

class TLSClient {
public:
    TLSClient(const char *ssid, const char *pw);
    TLSClient(const TLSClient&) = delete;
    ~TLSClient();

    int reconnect_wifi(const char *ssid, const char *pw);

    // DNS-resolve hostname and open a TLS connection. 0 on success, <0 on error.
    int connect(const char *hostname, int port, int timeout_ms);
    int write(char *buffer, int len);
    int read(char *buffer, int len);

private:
    static err_t tls_connected(void *arg, struct altcp_pcb *pcb, err_t err);
    static err_t tls_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
    static err_t tls_poll(void *arg, struct altcp_pcb *pcb);
    static err_t tls_disconnect(void* arg);
    static void  tls_err(void *arg, err_t err);
    static void  dns_found(const char *name, const ip_addr_t *ipaddr, void *arg);
    static void  tls_connect_to_server_ip(const ip_addr_t *ipaddr, void *arg);

    struct altcp_tls_config *tls_config;
    struct altcp_pcb *pcb;

    
    static constexpr int BUF_SIZE{4096};
    char buffer[BUF_SIZE];
    int count;

    char* req;
    size_t req_s;

    TaskHandle_t to_notify;

    ip_addr_t     remote_addr;
};

#endif // CO2CONTROLLER_TLS_CLIENT_H
