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
#include <lx_kit/map.h>
#include <lx_kit/byte_range.h>


Genode::Attached_dataspace & Lx_kit::Mem_allocator::alloc_dataspace(size_t size)
{
	Ram_dataspace_capability ds_cap;

	try {
		Ram_dataspace_capability ds_cap =
			_platform.alloc_dma_buffer(align_addr(size, 12), _cache_attr);

		Buffer & buffer = *new (_heap)
			Buffer(_env.rm(), ds_cap, _platform.dma_addr(ds_cap));

		/* map eager by touching all pages once */
		for (size_t sz = 0; sz < buffer.size(); sz += 4096) {
			touch_read((unsigned char const volatile*)(buffer.virt_addr() + sz)); }

		_virt_to_dma.insert(buffer.virt_addr(), buffer);
		_dma_to_virt.insert(buffer.dma_addr(), buffer);

		return buffer.ds();
	} catch (Out_of_caps) {
		_platform.free_dma_buffer(ds_cap);
		throw;
	}
}


void Lx_kit::Mem_allocator::free_dataspace(void * addr)
{
	Buffer *buffer = nullptr;

	_virt_to_dma.apply(Buffer_info::Query_addr(addr),
	                   [&] (Buffer_info const & info) {
		buffer = &info.buffer;
	});

	if (!buffer) {
		warning(__func__, ": no buffer for addr: ", addr, " found");
		return;
	}

	void const * virt_addr = (void const *)buffer->virt_addr();
	void const * dma_addr  = (void const *)buffer->dma_addr();

	_virt_to_dma.remove(Buffer_info::Query_addr(virt_addr));
	_dma_to_virt.remove(Buffer_info::Query_addr(dma_addr));

	Ram_dataspace_capability ds_cap = buffer->ram_ds_cap();

	destroy(_heap, buffer);

	_platform.free_dma_buffer(ds_cap);
}


Genode::Dataspace_capability Lx_kit::Mem_allocator::attached_dataspace_cap(void * addr)
{
	Genode::Dataspace_capability ret { };

	_virt_to_dma.apply(Buffer_info::Query_addr(addr),
	                   [&] (Buffer_info const & info) {
		ret = info.buffer.ds().cap();
	});

	return ret;
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
		Attached_dataspace & ds = alloc_dataspace(max(size + 1,
		                                          min_buffer_size));

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

	_virt_to_dma.apply(Buffer_info::Query_addr(addr),
	                   [&] (Buffer_info const & info) {
		addr_t const offset = (addr_t)addr - info.buffer.virt_addr();
		ret = info.buffer.dma_addr() + offset;
	});

	return ret;
}


Genode::addr_t Lx_kit::Mem_allocator::virt_addr(void * dma_addr)
{
	addr_t ret = 0UL;

	_dma_to_virt.apply(Buffer_info::Query_addr(dma_addr),
	                   [&] (Buffer_info const & info) {
		addr_t const offset = (addr_t)dma_addr - info.buffer.dma_addr();
		ret = info.buffer.virt_addr() + offset;
	});

	return ret;
}


bool Lx_kit::Mem_allocator::free(const void * ptr)
{
	if (!_mem.valid_addr((addr_t)ptr))
		return false;

	if (!_mem.size_at(ptr))
		return true;

	_mem.free(const_cast<void*>(ptr));
	return true;
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
