#include <cozis/endian.h>

bool cpu_is_little_endian(void)
{
    u16 x = 1;
    return *((u8*) &x);
}

static u16 invert_byte_order_u16(u16 n)
{
    return (n >> 8) 
         | (n << 8);
}

static u32 invert_byte_order_u32(u32 n)
{
    return ((n >> 24) & 0x000000FF)
         | ((n >>  8) & 0x0000FF00)
         | ((n <<  8) & 0x00FF0000)
         | ((n << 24) & 0xFF000000);
}

u16 net_to_cpu_u16(u16 n)
{
    if (cpu_is_little_endian())
        return invert_byte_order_u16(n);
    return n;
}

u32 net_to_cpu_u32(u32 n)
{
    if (cpu_is_little_endian())
        return invert_byte_order_u32(n);
    return n;
}

u16 cpu_to_net_u16(u16 n)
{
    if (cpu_is_little_endian())
        return invert_byte_order_u16(n);
    return n;
}

u32 cpu_to_net_u32(u32 n)
{
    if (cpu_is_little_endian())
        return invert_byte_order_u32(n);
    return n;
}

