#ifndef COZIS_ENDIAN_H
#define COZIS_ENDIAN_H

#include <cozis/types.h>

bool cpu_is_little_endian(void);
u16 net_to_cpu_u16(u16 n);
u32 net_to_cpu_u32(u32 n);
u16 cpu_to_net_u16(u16 n);
u32 cpu_to_net_u32(u32 n);

#endif
