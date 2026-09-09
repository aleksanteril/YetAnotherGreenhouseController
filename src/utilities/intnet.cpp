#include "intnet.h"

void
inet_to_dec(uint32_t ip, uint8_t* a, uint8_t* b, uint8_t* c, uint8_t* d)
{
        *a = (uint8_t)(ip >> 24U);
        *b = (uint8_t)(ip >> 16U);
        *c = (uint8_t)(ip >> 8U);
        *d = (uint8_t)ip;
}

uint32_t
dec_to_inet(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
        uint32_t ipaddr{};
        ipaddr = ((uint32_t)a << 24U) | 
                 ((uint32_t)b << 16U) |
                 ((uint32_t)c << 8U)  | 
                  (uint32_t) d;
        return ipaddr;
}

bool
valid_ip(uint32_t ipaddr)
{
        uint8_t first_octet{ (uint8_t)(ipaddr >> 24U) };
        if (first_octet == 0) {
                return false;
        } else if(first_octet == 127) {
                return false;
        } else if(first_octet > 223) {
                return false;
        }
        return true;
}
