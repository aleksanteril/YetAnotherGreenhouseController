#include "TlsClient.h"

#include <cstring>
#include <cstdio>
#include "pico/time.h"

#include "lwip/dns.h"
#include "mbedtls/ssl.h"

static const char THINGSPEAK_CERT[] =
"-----BEGIN CERTIFICATE-----\n\
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n\
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n\
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n\
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n\
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n\
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n\
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n\
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n\
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n\
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n\
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n\
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n\
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n\
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n\
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n\
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n\
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n\
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n\
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n\
MrY=\n\
-----END CERTIFICATE-----\n";


TlsClient::TlsClient(const char *ssid, const char *pw)
    : tls_config(nullptr), pcb(nullptr), count(0), rd(0), connected(false), closed(false), dns_done(false) {

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pw, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect.\n");
    } else {
        printf("Connected.\n");
    }

    tls_config = altcp_tls_create_config_client((const uint8_t *)THINGSPEAK_CERT, sizeof(THINGSPEAK_CERT));
    if (!tls_config) {
        printf("Failed to create TLS config\n");
        return;
    }
    mbedtls_ssl_conf_authmode((mbedtls_ssl_config *)tls_config, MBEDTLS_SSL_VERIFY_REQUIRED);
}

TlsClient::~TlsClient() {
    disconnect();
    if (tls_config) {
        altcp_tls_free_config(tls_config);
        tls_config = nullptr;
    }
}

int TlsClient::reconnect_wifi(const char *ssid, const char *pw) {
    return cyw43_arch_wifi_connect_timeout_ms(ssid, pw, CYW43_AUTH_WPA2_AES_PSK, 30000);
}


void TlsClient::dns_found(const char *name, const ip_addr_t *ipaddr, void *arg) {
    (void)name;
    auto *self = static_cast<TlsClient *>(arg);
    if (ipaddr) {
        self->remote_addr = *ipaddr;
    } else {
        self->closed = true;
    }
    self->dns_done = true;
}


err_t TlsClient::tls_connected(void *arg, altcp_pcb *pcb, err_t err) {
    (void)pcb;
    auto *self = static_cast<TlsClient *>(arg);
    if (err != ERR_OK) {
        self->closed = true;
        return err;
    }
    self->connected = true;
    return ERR_OK;
}

err_t TlsClient::tls_recv(void *arg, altcp_pcb *pcb, pbuf *p, err_t err) {
    (void)err;
    auto *self = static_cast<TlsClient *>(arg);
    if (!p) {
        self->closed = true;
        return ERR_OK;
    }
    if (p->tot_len > 0) {
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

err_t TlsClient::tls_poll(void *arg, altcp_pcb *pcb) {
    (void)pcb;
    (void)arg;
    return ERR_OK;
}

void TlsClient::tls_err(void *arg, err_t err) {
    (void)err;
    auto *self = static_cast<TlsClient *>(arg);
    self->pcb = nullptr; // already freed by lwIP
    self->closed = true;
}

int TlsClient::connect(const char *hostname, int port) {
    if (!tls_config) {
        return -1;
    }

    count = 0;
    rd = 0;
    connected = false;
    closed = false;
    dns_done = false;

    pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
    if (!pcb) {
        printf("failed to create pcb\n");
        return -2;
    }

    altcp_arg(pcb, this);
    altcp_recv(pcb, tls_recv);
    altcp_poll(pcb, tls_poll, 10);
    altcp_err(pcb, tls_err);

    // SNI
    mbedtls_ssl_set_hostname((mbedtls_ssl_context *)altcp_tls_context(pcb), hostname);

    // DNS resolve
    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(hostname, &remote_addr, dns_found, this);
    cyw43_arch_lwip_end();

    if (err == ERR_OK) {
        dns_done = true; // cached
    } else if (err != ERR_INPROGRESS) {
        printf("DNS error %d\n", err);
        disconnect();
        return -3;
    }

    absolute_time_t to = make_timeout_time_ms(10000);
    while (!dns_done && !time_reached(to)) {
        cyw43_arch_poll();
    }
    if (!dns_done || closed) {
        printf("DNS resolve failed for %s\n", hostname);
        disconnect();
        return -4;
    }

    cyw43_arch_lwip_begin();
    err = altcp_connect(pcb, &remote_addr, (u16_t)port, tls_connected);
    cyw43_arch_lwip_end();
    if (err != ERR_OK) {
        printf("altcp_connect error %d\n", err);
        disconnect();
        return -5;
    }

    to = make_timeout_time_ms(15000);
    while (!connected && !closed && !time_reached(to)) {
        cyw43_arch_poll();
    }
    if (!connected || closed) {
        printf("TLS handshake failed\n");
        disconnect();
        return -6;
    }

    return 0;
}


int TlsClient::write(unsigned char *buffer, int len, int timeout) {
    (void)timeout;
    if (!pcb || !connected || closed) {
        return -1;
    }
    cyw43_arch_lwip_begin();
    err_t err = altcp_write(pcb, buffer, (u16_t)len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        err = altcp_output(pcb);
    }
    cyw43_arch_lwip_end();
    if (err != ERR_OK) {
        printf("altcp_write error %d\n", err);
        return -2;
    }
    return len;
}


int TlsClient::read(unsigned char *buffer, int len, int timeout) {
    absolute_time_t to = make_timeout_time_ms(timeout);
    while (count == 0 && !closed && !time_reached(to)) {
        cyw43_arch_poll();
    }
    int to_copy = count < len ? count : len;
    if (to_copy > 0) {
        std::memcpy(buffer, this->buffer + rd, to_copy);
        rd += to_copy;
        count -= to_copy;
        if (count == 0) {
            rd = 0;
        }
    }
    return to_copy;
}


int TlsClient::disconnect() {
    cyw43_arch_lwip_begin();
    if (pcb != nullptr) {
        altcp_arg(pcb, nullptr);
        altcp_recv(pcb, nullptr);
        altcp_poll(pcb, nullptr, 0);
        altcp_err(pcb, nullptr);
        err_t err = altcp_close(pcb);
        if (err != ERR_OK) {
            altcp_abort(pcb);
        }
        pcb = nullptr;
    }
    cyw43_arch_lwip_end();
    connected = false;
    closed = false;
    count = 0;
    rd = 0;
    return 0;
}
