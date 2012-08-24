/*
 * \brief  Memory pool
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MEM_H_
#define _MEM_H_

#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <lx_emul.h>

/*********************
 ** linux/dmapool.h **
 *********************/

namespace Genode {

	/**
	 * Memory back-end
	 */
	class Mem
	{
		/* configurable sizes of memory pools */
		enum
		{ 
			MEM_POOL_SHARE = 3
		};

		private:

			addr_t        _base;       /* virt base of pool */
			addr_t        _base_phys;  /* phys base of pool */
			size_t        _size;       /* size of backng store */
			Allocator_avl _range;      /* range allocator for pool */
			addr_t       *_zones;      /* bases of zones */
			int           _zone_count; /* number of zones */
			int           _zone_alloc; /* currently allocated zones */

			Ram_dataspace_capability _ds_cap; /* backing store */

			/**
			 * Private constructor
			 */
			Mem(size_t size, bool cached = true)
			: _size(size), _range(env()->heap()),_zone_count(0), _zone_alloc(0)
			{
				_ds_cap    = env()->ram_session()->alloc(_size, cached);
				_base_phys = Dataspace_client(_ds_cap).phys_addr();
				_base      = (addr_t)env()->rm_session()->attach(_ds_cap);

				dde_kit_log(DEBUG_DMA, "New DMA range [%lx-%lx)", _base, _base + _size);
				_range.add_range(_base, _size);
			}

			/**
			 * Convert 'Mem' addres to zone address
			 */
			void *_to_zone(void const *addr, int i)
			{
				if (i < 0)
					return (void *)addr;

				addr_t zone_base = _zones[i];
				addr_t offset    = (addr_t)addr - _base;
				return (void *)(zone_base + offset);
			}

			/**
			 * Convert zone addres to 'Mem' address
			 */
			void *_from_zone(void const *addr, int i)
			{
				if (i < 0)
					return (void *)addr;

				addr_t zone_base = _zones[i];
				addr_t offset    = (addr_t)addr - zone_base;
				return (void *)(_base + offset);
			}

			/**
			 * Memory usable by back-end allocators
			 */
			static size_t _mem_avail() {
				return env()->ram_session()->avail() - (1024 * 1024); }

		public:

			/**
			 * Memory zone within Mem allocator
			 */
			class Zone_alloc : public Allocator
			{
				private:

					Mem   *_pool; /* pool of zone */
					int    _zone; /* zone number */
					addr_t _base; /* base address of zone */
					size_t _size; /* size of zone */

				public:

					Zone_alloc(Mem *pool, int zone, addr_t base, size_t size)
					: _pool(pool), _zone(zone), _base(base), _size(size) { }


					/*************************
					 ** Alocator interface  **
					 *************************/

					bool alloc(size_t size, void **out_addr)
					{
						*out_addr = _pool->alloc(size, _zone);
						if (!*out_addr) {
							PERR("Zone of %zu bytes allocation failed", size);
							return false;
						}
						return true;
					}

					void   free(void *addr, size_t /* size */) { _pool->free(addr, _zone); }
					size_t overhead(size_t size) { return  0; }

					/**
					 * Check if address matches zone
					 */
					bool match(void const *addr)
					{
						addr_t a = (addr_t)addr;
						bool ret = ((a >= _base) && (a < (_base + _size)));
						return ret;
					}

					/**
					 * Retrieve virt to phys mapping
					 */
					addr_t phys_addr(void const *addr) { return _pool->phys_addr(addr, _zone); }
			};

			/**
			 * Gernal purpose memory pool
			 */
			static Mem* pool()
			{
				static Mem _p(_mem_avail() / MEM_POOL_SHARE);
				return &_p;
			}


			/**
			 * DMA memory pool
			 */
			static Mem* dma()
			{
				static Mem _p(_mem_avail() - (_mem_avail() / MEM_POOL_SHARE), false);
				return &_p;
			}


			/**
			 * Allocator interface
			 */
			void *alloc(size_t size, int zone = -1, int align = 2)
			{
				void *addr;
				if (!_range.alloc_aligned(size, &addr, align)) {
					PERR("Memory allocation of %zu bytes failed", size);
					return 0;
				}

				return _to_zone(addr, zone);
			}

			/**
			 * Free addr in zone
			 */
			void free(void *addr, int zone = -1)  { _range.free(_from_zone(addr, zone)); }

			/**
			 * Get phys for virt address
			 */
			addr_t phys_addr(void const *addr, int zone = - 1)
			{
				addr_t a = (addr_t)_from_zone(addr, zone);
				if (a < _base || a >= _base + _size) {
					PERR("No DMA phys addr for %lx zone: %d", a, zone);
					return 0;
				}
				return (a - _base) + _base_phys;
			}

			/**
			 * Iinit allocator with count zones
			 */
			void init_zones(int count)
			{
				if (_zone_count)
					return;

				_zones = (addr_t *)env()->heap()->alloc(count * sizeof(addr_t));
				_zone_count = count;

				for (int i = 0; i < _zone_count; i++) {
					_zones[i] = (addr_t)env()->rm_session()->attach(_ds_cap);
					dde_kit_log(DEBUG_DMA, "Zone %d: base: %lx end %lx", i, _zones[i], _zones[i] + _size);
				}

				PINF("Registered %d zone allocators", count);
			}

			/**
			 * Create new zone allocator
			 *
			 * 'init_zones' must have been called beforehand
			 */
			Zone_alloc *new_zone_allocator()
			{
				if(_zone_alloc >= _zone_count) {
					PERR("Zone allocators exhausted");
					return 0;
				}

				Zone_alloc *zone = new(env()->heap()) Zone_alloc(this,
				                                                 _zone_alloc,
				                                                 _zones[_zone_alloc],
				                                                 _size);
				_zone_alloc++;
				return zone;
			}
	};
}

#endif /* _MEM_H_ */
