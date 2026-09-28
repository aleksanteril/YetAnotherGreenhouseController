#include "TLSClient.h"

#include <cstring>
#include <cstdio>
#include "pico/time.h"

#include "lwip/dns.h"
#include "mbedtls/ssl.h"

//#define DEBUG_printf printf
#define DEBUG_printf

TLSClient::TLSClient(const char *ssid, const char *pw)
    : tls_config(nullptr), pcb(nullptr), count(0), rd(0), to_notify(nullptr) {

    if (cyw43_arch_init()) {
        DEBUG_printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

    DEBUG_printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pw, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        DEBUG_printf("Failed to connect.\n");
    } else {
        DEBUG_printf("Connected.\n");
    }

    tls_config = altcp_tls_create_config_client(nullptr, 0);
    if (!tls_config) {
        DEBUG_printf("Failed to create TLS config\n");
        return;
    }
    mbedtls_ssl_conf_authmode((mbedtls_ssl_config *)tls_config, MBEDTLS_SSL_VERIFY_NONE);
}

TLSClient::~TLSClient() {
    tls_disconnect(this);
    if (tls_config) {
        altcp_tls_free_config(tls_config);
        tls_config = nullptr;
    }
}

int TLSClient::reconnect_wifi(const char *ssid, const char *pw) {
    return cyw43_arch_wifi_connect_timeout_ms(ssid, pw, CYW43_AUTH_WPA2_AES_PSK, 30000);
}

void 
TLSClient::tls_connect_to_server_ip(const ip_addr_t *ipaddr, void *arg)
{
    auto *self = static_cast<TLSClient *>(arg);
    DEBUG_printf("connecting to server IP %s port %d\n", ipaddr_ntoa(ipaddr), 443);
    err_t err = altcp_connect(self->pcb, ipaddr, (u16_t)443, tls_connected);
    if (err != ERR_OK)
    {
        DEBUG_printf("error initiating connect, err=%d\n", err);
        tls_disconnect(self);
    }
}

void TLSClient::dns_found(const char *name, const ip_addr_t *ipaddr, void *arg) {
    (void)name;
    auto *self = static_cast<TLSClient *>(arg);
    if (ipaddr) {
        tls_connect_to_server_ip(ipaddr, arg);
    } else {
        DEBUG_printf("error resolving dns\n");
        tls_disconnect(arg);
    }
}


err_t TLSClient::tls_connected(void *arg, altcp_pcb *pcb, err_t err) {
    (void)pcb;
    auto *self = static_cast<TLSClient *>(arg);
    if (err != ERR_OK) {
        // connection fail
        return tls_disconnect(arg);
    }
    err = altcp_write(self->pcb, self->req, self->req_s, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("error writing data, err=%d", err);
        return tls_disconnect(arg);
    }
    return ERR_OK;
}

err_t TLSClient::tls_recv(void *arg, altcp_pcb *pcb, pbuf *p, err_t err) {
    (void)err;
    auto *self = static_cast<TLSClient *>(arg);
    if (!p) {
        tls_disconnect(arg);
        return ERR_OK;
    }
    if (p->tot_len > 0) {
        DEBUG_printf("received %dB\n", p->tot_len);
        int write_pos = self->rd + self->count;
        int space = BUF_SIZE - write_pos;
        int to_copy = p->tot_len < space ? p->tot_len : space;
        if (to_copy > 0) {
            pbuf_copy_partial(p, self->buffer + write_pos, to_copy, 0);
            self->count += to_copy;
        }
        altcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

err_t TLSClient::tls_poll(void *arg, altcp_pcb *pcb) {
    return tls_disconnect(arg);
}

void TLSClient::tls_err(void *arg, err_t err) {
    tls_disconnect(arg);
}

int TLSClient::connect(const char *hostname, int port=443, int timeout_ms=15000) {
    if (!tls_config) {
        return -1;
    }

    pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
    if (!pcb) {
        DEBUG_printf("failed to create pcb\n");
        return -2;
    }

    // if this would be used from many tasks, we need a mutex here 
    // but this is only used in 1 task that waits for the lwIP task to complete
    // so be it
    count = 0;
    rd = 0;
    to_notify = xTaskGetCurrentTaskHandle();

    altcp_arg(pcb, this);
    altcp_recv(pcb, tls_recv);
    altcp_poll(pcb, tls_poll, 10);
    altcp_err(pcb, tls_err);

    // SNI
    mbedtls_ssl_set_hostname((mbedtls_ssl_context *)altcp_tls_context(pcb), hostname);

    // DNS resolve
    cyw43_arch_lwip_begin();

    err_t err = dns_gethostbyname(hostname, &remote_addr, dns_found, this);
    if (err == ERR_OK) {
        tls_connect_to_server_ip(&remote_addr, this);
    } else if (err != ERR_INPROGRESS) {
        DEBUG_printf("DNS error %d\n", err);
        tls_disconnect(this);
        return -3;
    }

    cyw43_arch_lwip_end();

    // wait for connection to close
    auto timeout{ pdMS_TO_TICKS(timeout_ms) };
    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == 0) {
        // timed out
        DEBUG_printf("connect timeout %d\n", err);
        tls_disconnect(this);
        return -4;
    }

    return 0;
}


int TLSClient::write(char *buffer, int len) {
    req = buffer;
    req_s = len;
    return len;
}


int TLSClient::read(char *buffer, int len) {
    int to_copy = count < len ? count : len;
    if (to_copy > 0) {
        // this copy from buffer to buffer is redundant as of the use case now
        // we could just pass the pointer of the resp buffer of TLSClient aswell
        std::memcpy(buffer, this->buffer, to_copy);
    }
    return to_copy;
}

err_t TLSClient::tls_disconnect(void* arg) {
    auto *self = static_cast<TLSClient *>(arg);
    err_t err = ERR_OK;
    DEBUG_printf("deleting pcb\n");
    // give signal connection closing
    xTaskNotifyGive(self->to_notify);
    cyw43_arch_lwip_begin();
    if (self->pcb != nullptr) {
        altcp_arg(self->pcb, nullptr);
        altcp_recv(self->pcb, nullptr);
        altcp_poll(self->pcb, nullptr, 0);
        altcp_err(self->pcb, nullptr);
        err_t err = altcp_close(self->pcb);
        if (err != ERR_OK) {
            err = ERR_ABRT;
            altcp_abort(self->pcb);
        }
        self->pcb = nullptr;
    }
    cyw43_arch_lwip_end();
    return err;
}
