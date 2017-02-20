/*
 * \brief  Genode base for jitterentropy
 * \author Josef Soentgen
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator.h>
#include <util/string.h>

/* local includes */
#include <jitterentropy-base-genode.h>


static Genode::Allocator *_alloc;


void jitterentropy_init(Genode::Allocator &alloc)
{
	_alloc = &alloc;
}


void *jent_zalloc(size_t len)
{
	if (!_alloc) { return 0; }
	return _alloc->alloc(len);
}


void jent_zfree(void *ptr, unsigned int len)
{
	if (!_alloc) { return; }
	_alloc->free(ptr, 0);
}


void *memcpy(void *dest, const void *src, size_t n)
{
	return Genode::memcpy(dest, src, n);
}
