/*
 * \brief  Lx_kit memory allocation backend
 * \author Stefan Kalkowski
 * \date   2021-03-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <os/backtrace.h>
#include <platform_session/connection.h>
#include <util/touch.h>

/* local includes */
#include <lx_kit/memory.h>


Genode::Attached_dataspace & Lx_kit::Mem_allocator::alloc_dataspace(size_t size)
{
	Ram_dataspace_capability ds_cap;

	try {
		size_t ds_size  = align_addr(size, 12);
		ds_cap          = _platform.alloc_dma_buffer(ds_size, _cache_attr);
		addr_t dma_addr = _platform.dma_addr(ds_cap);

		Buffer & buffer = *new (_heap)
			Registered<Buffer>(_buffers, _env.rm(), ds_cap, dma_addr);
		addr_t addr     = (addr_t)buffer.ds().local_addr<void>();

		/* map eager by touching all pages once */
		for (size_t sz = 0; sz < ds_size; sz += 4096) {
			touch_read((unsigned char const volatile*)(addr + sz)); }

		return buffer.ds();
	} catch (Out_of_caps) {
		_platform.free_dma_buffer(ds_cap);
		throw;
	}
}


void * Lx_kit::Mem_allocator::alloc(size_t size, size_t align)
{
	if (!size)
		return nullptr;

	void * out_addr = nullptr;

	if (_mem.alloc_aligned(size, &out_addr, log2(align)).error()) {

		/*
		 * Restrict the minimum buffer size to avoid the creation of
		 * a separate dataspaces for tiny allocations.
		 */
		size_t const min_buffer_size = 256*1024;

		/*
		 * Allocate one excess byte that is not officially registered at
		 * the '_mem' ranges. This way, two virtual consecutive ranges
		 * (that must be assumed to belong to non-contiguous physical
		 * ranges) can never be merged when freeing an allocation. Such
		 * a merge would violate the assumption that a both the virtual
		 * and physical addresses of a multi-page allocation are always
		 * contiguous.
		 */
		Attached_dataspace & ds = alloc_dataspace(max(size + 1, min_buffer_size));

		_mem.add_range((addr_t)ds.local_addr<void>(), ds.size() - 1);

		/* re-try allocation */
		_mem.alloc_aligned(size, &out_addr, log2(align));
	}

	if (!out_addr) {
		error("memory allocation failed for ", size, " align ", align);
		backtrace();
	}
	else
		memset(out_addr, 0, size);

	return out_addr;
}


Genode::addr_t Lx_kit::Mem_allocator::dma_addr(void * addr)
{
	addr_t ret = 0UL;

	_buffers.for_each([&] (Buffer & b) {
		addr_t other = (addr_t)addr;
		addr_t addr  = (addr_t)b.ds().local_addr<void>();
		if (addr > other || (addr+b.ds().size()) <= other)
			return;

		/* byte offset of 'addr' from start of block */
		addr_t const offset = other - addr;
		ret = b.dma_addr() + offset;
	});

	return ret;
}


bool Lx_kit::Mem_allocator::free(const void * ptr)
{
	if (!_mem.valid_addr((addr_t)ptr))
		return false;

	_mem.free(const_cast<void*>(ptr));
	return true;
}


void Lx_kit::Mem_allocator::free(Attached_dataspace * ds)
{
	Dataspace_capability cap = ds->cap();

	Registered<Buffer> * buffer = nullptr;
	_buffers.for_each([&] (Buffer & b) {
		if (&b.ds() == ds)
			buffer = static_cast<Registered<Buffer>*>(&b);
	});

	destroy(_heap, buffer);
	_platform.free_dma_buffer(static_cap_cast<Ram_dataspace>(cap));
}


Genode::size_t Lx_kit::Mem_allocator::size(const void * ptr)
{
	return ptr ? _mem.size_at(ptr) : 0;
}


Lx_kit::Mem_allocator::Mem_allocator(Genode::Env          & env,
                                     Heap                 & heap,
                                     Platform::Connection & platform,
                                     Cache                  cache_attr)
:
	_env(env), _heap(heap), _platform(platform),
	_cache_attr(cache_attr), _mem(&heap) {}
