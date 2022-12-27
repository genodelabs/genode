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

#ifndef _LX_KIT__MEMORY_H_
#define _LX_KIT__MEMORY_H_

#include <base/allocator_avl.h>
#include <base/cache.h>
#include <base/env.h>
#include <base/heap.h>
#include <lx_kit/byte_range.h>
#include <lx_kit/map.h>

namespace Platform { class Connection; }

namespace Lx_kit {
	using namespace Genode;
	class Mem_allocator;
}


class Lx_kit::Mem_allocator
{
	public:

		struct Buffer
		{
			virtual ~Buffer() {}

			virtual size_t dma_addr()  const   = 0;
			virtual size_t size()      const   = 0;
			virtual size_t virt_addr() const   = 0;
			virtual Dataspace_capability cap() = 0;
		};

	private:

		struct Buffer_info
		{
			struct Key { addr_t addr; } key;
			Buffer & buffer;

			size_t size() const { return buffer.size(); }

			bool higher(Key const other_key) const
			{
				return key.addr > other_key.addr;
			}

			struct Query_range
			{
				addr_t addr;
				size_t size;

				bool matches(Buffer_info const & bi) const
				{
					Lx_kit::Byte_range buf_range { bi.key.addr, bi.size() };
					Lx_kit::Byte_range range     { addr, size };

					return buf_range.intersects(range);
				}

				Key key() const { return Key { addr }; }
			};

			struct Query_addr : Query_range
			{
				Query_addr(void const * addr)
				: Query_range{(addr_t)addr, 1} { }
			};
		};

		Env                  & _env;
		Heap                 & _heap;
		Platform::Connection & _platform;
		Cache                  _cache_attr;
		Allocator_avl          _mem         { &_heap };
		Map<Buffer_info>       _virt_to_dma {  _heap };
		Map<Buffer_info>       _dma_to_virt {  _heap };

	public:

		Mem_allocator(Env                  & env,
		              Heap                 & heap,
		              Platform::Connection & platform,
		              Cache                  cache_attr);

		Buffer             & alloc_buffer(size_t size);
		void                 free_buffer(void *addr);
		Dataspace_capability attached_dataspace_cap(void *addr);

		void * alloc(size_t size, size_t align);
		addr_t dma_addr(void * addr);
		addr_t virt_addr(void * dma_addr);
		addr_t virt_region_start(void * virt_addr);
		size_t size(const void * ptr);
		bool   free(const void * ptr);
};

#endif /* _LX_KIT__MEMORY_H_ */
