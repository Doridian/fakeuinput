#pragma once

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define BITS_PER_LONG (sizeof(long) * BITS_PER_BYTE)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define set_bit(bit, array)	    ((array[LONG(bit)] |= BIT(bit)))
#define clear_bit(bit, array)	((array[LONG(bit)] &= ~BIT(bit)))
