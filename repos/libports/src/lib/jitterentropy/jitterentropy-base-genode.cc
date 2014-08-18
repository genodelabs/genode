/*
 * \brief  Genode base for jitterentropy
 * \author Josef Soentgen
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>

/* local includes */
#include <jitterentropy-base-genode.h>


void *jent_zalloc(size_t len)
{
	return Genode::env()->heap()->alloc(len);
}


void jent_zfree(void *ptr, unsigned int len)
{
	Genode::env()->heap()->free(ptr, len);
}


void *memcpy(void *dest, const void *src, size_t n)
{
	return Genode::memcpy(dest, src, n);
}
