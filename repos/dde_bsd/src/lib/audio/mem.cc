/**
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <util/string.h>

/* local includes */
#include <bsd.h>
#include <bsd_emul.h>


static bool const verbose = false;


static Genode::Allocator *_malloc;


void Bsd::mem_init(Genode::Env &env, Genode::Allocator &)
{
	/*
	 * The total amount of memory is small (around 140 KiB) and
	 * static throughout the drivers life-time. Although a mix
	 * of very small (4B) and some larger (32 KiB) allocations
	 * is performed, the heap is good enough.
	 */
	static Genode::Heap m(env.ram(), env.rm());

	_malloc = &m;
}


static Genode::Allocator &malloc_backend() { return *_malloc; }


/**********************
 ** Memory allocation *
 **********************/

static size_t _mallocarray_dummy;

extern "C" void *malloc(size_t size, int type, int flags)
{
	void *addr = malloc_backend().alloc(size);

	if (addr && (flags & M_ZERO))
		Genode::memset(addr, 0, size);

	return addr;
}


extern "C" void *mallocarray(size_t nmemb, size_t size, int type, int flags)
{
	if (size != 0 && nmemb > (~0UL / size))
		return 0;

	/*
	 * The azalia codec code might call mallocarray with
	 * nmemb == 0 as 'nopin' etc. can be 0. The allocation
	 * will not be used, so simple return a dummy as NULL
	 * will be treated as ENOMEM.
	 */
	if (nmemb == 0 || size == 0)
		return &_mallocarray_dummy;

	return malloc(nmemb * size, type, flags);
}


extern "C" void free(void *addr, int type, size_t size)
{
	if (!addr) return;

	if (addr == &_mallocarray_dummy)
		return;

	malloc_backend().free(addr, size);
}


/*****************
 ** sys/systm.h **
 *****************/

extern "C" void bzero(void *b, size_t len)
{
	if (b == nullptr) return;

	Genode::memset(b, 0, len);
}


extern "C" void bcopy(const void *src, void *dst, size_t len)
{
	/* XXX may overlap */
	Genode::memcpy(dst, src, len);
}


extern "C" int uiomove(void *buf, int n, struct uio *uio)
{
	void *dst = nullptr;
	void *src = nullptr;
	size_t len = uio->uio_resid < (size_t)n ? uio->uio_resid : (size_t)n;

	switch (uio->uio_rw) {
	case UIO_READ:
		dst = buf;
		src = ((char*)uio->buf) + uio->uio_offset;
		break;
	case UIO_WRITE:
		dst = ((char*)uio->buf) + uio->uio_offset;
		src = buf;
		break;
	}

	Genode::memcpy(dst, src, len);

	uio->uio_resid  -= len;
	uio->uio_offset += len;

	return 0;
}
