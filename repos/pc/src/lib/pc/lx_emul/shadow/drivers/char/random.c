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


void add_input_randomness(unsigned int type,unsigned int code,unsigned int value)
{
	lx_emul_trace(__func__);
}


u32 get_random_u32(void)
{
	return lx_emul_gen_random_u32();
}


u64 get_random_u64(void)
{
	return lx_emul_gen_random_u64();
}


int __must_check get_random_bytes_arch(void *buf, int nbytes)
{
	if (nbytes < 0)
		return -1;

	lx_emul_gen_random_bytes(buf, nbytes);
	return nbytes;
}


void get_random_bytes(void *buf, int nbytes)
{
	nbytes = get_random_bytes_arch(buf, nbytes);
}
