#ifndef INTNET_H
#define INTNET_H

#include "pico/types.h"

void
inet_to_dec(uint32_t ip, uint8_t* a, uint8_t* b, uint8_t* c, uint8_t* d);
uint32_t
dec_to_inet(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

#endif
