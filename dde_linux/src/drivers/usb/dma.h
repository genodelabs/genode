/*
 * \brief  DMA memory pool
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DMA_H_
#define _DMA_H_

#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <lx_emul.h>

/*********************
 ** linux/dmapool.h **
 *********************/

namespace Genode {

	/**
	 * Dma-pool manager
	 */
	class Dma
	{
		private:

			enum { SIZE = 1024 * 1024 };

			addr_t        _base;       /* virt base of pool */
			addr_t        _base_phys;  /* phys base of pool */
			Allocator_avl _range;      /* range allocator for pool */

			Dma() : _range(env()->heap())
			{
				Ram_dataspace_capability cap = env()->ram_session()->alloc(SIZE, false);
				_base_phys = Dataspace_client(cap).phys_addr();
				_base      = (addr_t)env()->rm_session()->attach(cap);
				dde_kit_log(DEBUG_DMA, "New DMA range [%lx-%lx)", _base, _base + SIZE);
				_range.add_range(_base, SIZE);
			}

		public:

			static Dma* pool()
			{
				static Dma _p;
				return &_p;
			}

			addr_t base() const { return _base; };
			addr_t end() const { return _base + SIZE -1; }

			/**
			 * Alloc 'size' bytes of DMA memory
			 */
			void *alloc(size_t size, int align = 12)
			{
				void *addr;
				if (!_range.alloc_aligned(size, &addr, align)) {
					PERR("DMA of %zu bytes allocation failed", size);
					return 0;
				}

				return addr;
			}

			/**
			 * Free DMA memory
			 */
			void  free(void *addr) { _range.free(addr); }

			/**
			 * Get phys for virt address
			 */
			addr_t phys_addr(void *addr)
			{
				addr_t a = (addr_t)addr;
				if (a < _base || a >= _base + SIZE) {
					PERR("No DMA phys addr for %lx", a);
					return 0;
				}
				return (a - _base) + _base_phys;
			}
	};
}

#endif /* _DMA_H_ */
