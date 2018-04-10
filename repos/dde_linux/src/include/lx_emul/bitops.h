/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************************
 ** asm-generic/bitsperlong.h **
 *******************************/

#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

#define BITS_PER_BYTE     8
#define BIT(nr)           (1UL << (nr))
#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define BIT_ULL(nr)   (1ULL << (nr))
#define BIT_MASK(nr)  (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)  ((nr) / BITS_PER_LONG)

#include <asm-generic/bitops/non-atomic.h>

#define test_and_clear_bit(nr, addr) \
        __test_and_clear_bit(nr, (volatile unsigned long *)(addr))
#define test_and_set_bit(nr, addr) \
        __test_and_set_bit(nr, (volatile unsigned long *)(addr))
#define set_bit(nr, addr) \
        __set_bit(nr, (volatile unsigned long *)(addr))
#define clear_bit(nr, addr) \
        __clear_bit(nr, (volatile unsigned long *)(addr))

#define smp_mb__before_clear_bit()
#define smp_mb__after_clear_bit() smp_mb()

/**
 * Find first zero bit (limit to machine word size)
 */
long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset);


#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/ffs.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/fls64.h>

static inline unsigned fls_long(unsigned long l)
{
	if (sizeof(l) == 4)
		return fls(l);
	return fls64(l);
}

static inline unsigned long __ffs64(u64 word)
{
#if BITS_PER_LONG == 32
	if (((u32)word) == 0UL)
		return __ffs((u32)(word >> 32)) + 32;
#elif BITS_PER_LONG != 64
#error BITS_PER_LONG not 32 or 64
#endif
	return __ffs((unsigned long)word);
}

#include <linux/log2.h>

#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size));        \
	     (bit) < (size);                    \
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

#define for_each_clear_bit(bit, addr, size) \
	for ((bit) = find_first_zero_bit((addr), (size));       \
	     (bit) < (size);                                    \
	     (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

static inline int get_bitmask_order(unsigned int count) {
	return __builtin_clz(count) ^ 0x1f; }

static inline __s32 sign_extend32(__u32 value, int index)
{
	__u8 shift = 31 - index;
	return (__s32)(value << shift) >> shift;
}

static inline __u32 rol32(__u32 word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

static inline __u32 ror32(__u32 word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

static inline __u16 ror16(__u16 word, unsigned int shift)
{
	return (word >> shift) | (word << (16 - shift));
}

#define BITS_PER_LONG_LONG (sizeof(long long) * 8)
#define GENMASK_ULL(h, l) \
	(((~0ULL) - (1ULL << (l)) + 1) & \
	 (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
