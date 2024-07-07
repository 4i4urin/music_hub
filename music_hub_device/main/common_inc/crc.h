#ifndef CRC_H
#define CRC_H

#include "base_types.h"

u16_t crc16(u16_t crc, const void *buf, u32_t size);

u8_t crc8(u8_t crc, const void *mem, u16_t len);

#endif /* CRC_H */