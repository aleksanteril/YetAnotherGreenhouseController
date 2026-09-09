#ifndef CRC16_H
#define CRC16_H

#include "pico/types.h"

uint16_t
crc16(const uint8_t* data_p, size_t length);

#endif
