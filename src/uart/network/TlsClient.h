
#ifndef CO2CONTROLLER_TLS_CLIENT_H
#define CO2CONTROLLER_TLS_CLIENT_H

#include <cstdint>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"

class TLSClient {
public:
    TLSClient(const char *ssid, const char *pw);
    ~TLSClient();

    int reconnect_wifi(const char *ssid, const char *pw);

    // DNS-resolve hostname and open a TLS connection. 0 on success, <0 on error.
    int connect(const char *hostname, int port);
    int write(unsigned char *buffer, int len, int timeout);
    int read(unsigned char *buffer, int len, int timeout);
    int disconnect();

    static const int BUF_SIZE{4096};

    static err_t tls_connected(void *arg, struct altcp_pcb *pcb, err_t err);
    static err_t tls_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
    static err_t tls_poll(void *arg, struct altcp_pcb *pcb);
    static void  tls_err(void *arg, err_t err);
    static void  dns_found(const char *name, const ip_addr_t *ipaddr, void *arg);

private:
    struct altcp_tls_config *tls_config;
    struct altcp_pcb *pcb;

    unsigned char buffer[BUF_SIZE];
    volatile int  count;
    int           rd;

    volatile bool connected;
    volatile bool closed;
    volatile bool dns_done;
    ip_addr_t     remote_addr;
};

#endif // CO2CONTROLLER_TLS_CLIENT_H
