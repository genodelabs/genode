/*
 * \brief  Genode base for jitterentropy
 * \author Josef Soentgen
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator.h>
#include <util/string.h>

#include <jitterentropy.h>


static Genode::Allocator *_alloc;


void jitterentropy_init(Genode::Allocator &alloc)
{
	_alloc = &alloc;
}


void *jent_zalloc(size_t len)
{
	if (!_alloc) { return 0; }

	void *p = _alloc->alloc(len);
	if (p)
		Genode::memset(p, 0, len);

	return p;
}


void jent_zfree(void *ptr, unsigned int)
{
	if (!_alloc) { return; }
	_alloc->free(ptr, 0);
}


void *jent_memcpy(void *dest, const void *src, size_t n)
{
	return Genode::memcpy(dest, src, n);
}


void *jent_memset(void *dest, int c, size_t n)
{
	return Genode::memset(dest, (uint8_t)c, n);
}
