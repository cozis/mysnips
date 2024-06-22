#include <cozis/bits.h>

// Returns the index from the right of the
// first set bit or -1 otherwise.
int find_first_set_bit(u64 bits)
{
    // First check that at least one bit is set
    if (bits == 0) return -1;
    
    u64 bits_no_rightmost = bits & (bits - 1);
    u64 bits_only_rightmost = bits - bits_no_rightmost;

    int index = 0;
    u64 temp;

    // The index of the rightmost bit is the log2
    temp = bits_only_rightmost >> 32;
    if (temp) {
        // Bit is in the upper 32 bits
        index += 32;
        bits_only_rightmost = temp;
    }

    temp = bits_only_rightmost >> 16;
    if (temp) {
        index += 16;
        bits_only_rightmost = temp;
    }

    temp = bits_only_rightmost >> 8;
    if (temp) {
        index += 8;
        bits_only_rightmost = temp;
    }

    static const u8 table[] = {
        0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
        4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    index += table[bits_only_rightmost];
    
    return index;
}

bool is_pow2(u64 n)
{
    return (n & (n-1)) == 0;
}
