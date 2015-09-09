/*
 * \brief  Slab allocator using our back-end allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__SLAB_ALLOC_H_
#define _LX_EMUL__IMPL__INTERNAL__SLAB_ALLOC_H_

/* Genode includes */
#include <base/slab.h>

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/slab_backend_alloc.h>

namespace Lx { class Slab_alloc; }

class Lx::Slab_alloc : public Genode::Slab
{
	private:

		/*
		 * Each slab block in the slab contains about 8 objects (slab entries)
		 * as proposed in the paper by Bonwick and block sizes are multiples of
		 * page size.
		 */
		static size_t _calculate_block_size(size_t object_size)
		{
			size_t block_size = 8 * (object_size + sizeof(Genode::Slab_entry))
			                                     + sizeof(Genode::Slab_block);
			return Genode::align_addr(block_size, 12);
		}

	public:

		Slab_alloc(size_t object_size, Slab_backend_alloc &allocator)
		: Slab(object_size, _calculate_block_size(object_size), 0, &allocator) { }

		/**
		 * Convenience slabe-entry allocation
		 */
		Genode::addr_t alloc()
		{
			Genode::addr_t result;
			return (Slab::alloc(slab_size(), (void **)&result) ? result : 0);
		}
};

#endif /* _LX_EMUL__IMPL__INTERNAL__SLAB_ALLOC_H_ */
