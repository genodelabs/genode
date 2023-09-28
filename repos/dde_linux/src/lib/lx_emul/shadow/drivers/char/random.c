/*
 * \brief  Replaces drivers/char/random.c
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/random.h>

#include <linux/random.h>
#include <linux/version.h>


void add_input_randomness(unsigned int type,unsigned int code,unsigned int value)
{
	lx_emul_trace(__func__);
}


u32 get_random_u32(void)
{
	return lx_emul_random_gen_u32();
}


u64 get_random_u64(void)
{
	return lx_emul_random_gen_u64();
}


int __must_check get_random_bytes_arch(void *buf, int nbytes)
{
	if (nbytes < 0)
		return -1;

	lx_emul_random_gen_bytes(buf, nbytes);
	return nbytes;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
void get_random_bytes(void *buf, int nbytes)
#else
void get_random_bytes(void *buf, size_t nbytes)
#endif
{
	nbytes = get_random_bytes_arch(buf, nbytes);
}


bool rng_is_initialized(void)
{
	return 1;
}


u32 __get_random_u32_below(u32 ceil)
{
	u32 div, result;
	/**
	 * Returns a random number from the half-open interval [0, ceil)
	 * with uniform distribution.
	 *
	 * The idea here is to split [0, 2^32) into #ceil bins. By dividing a random
	 * number from the 32-bit interval, we can determine into which bin the number
	 * fell.
	 */
	if (ceil <= 1) return 0; /* see 'get_random_u32_below' */

	/* determine divisor to determine bin number by dividing 2^32 by ceil */
	div = 0x100000000ULL / ceil;

	/**
	 * In case the above division has a remainder, we will end up with an
	 * additional (but smaller) bin at the end of the 32-bit interval. We'll
	 * discard the result if the number fell into this bin and repeat.
	 */
	result = ceil;
	while (result >= ceil) {
		result = lx_emul_random_gen_u32() / div;
	}

	return result;
}
