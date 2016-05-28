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

#ifndef _LX_KIT__INTERAL__SLAB_ALLOC_H_
#define _LX_KIT__INTERAL__SLAB_ALLOC_H_

/* Genode includes */
#include <base/slab.h>
#include <util/misc_math.h>

/* Linux emulation environment includes */
#include <lx_kit/internal/slab_backend_alloc.h>
#include <lx_kit/types.h>

namespace Lx { class Slab_alloc; }

class Lx::Slab_alloc : public Genode::Slab
{
	private:

		size_t const _object_size;

		/*
		 * Each slab block in the slab contains about 8 objects (slab entries)
		 * as proposed in the paper by Bonwick and block sizes are multiples of
		 * page size.
		 */
		static size_t _calculate_block_size(size_t object_size)
		{
			size_t block_size = 16*object_size;
			return Genode::align_addr(block_size, 12);
		}

	public:

		Slab_alloc(size_t object_size, Slab_backend_alloc &allocator)
		:
			Slab(object_size, _calculate_block_size(object_size), 0, &allocator),
			_object_size(object_size)
		{ }

		Genode::addr_t alloc()
		{
			Genode::addr_t result;
			return (Slab::alloc(_object_size, (void **)&result) ? result : 0);
		}

		void free(void *ptr)
		{
			Slab::free(ptr, _object_size);
		}
};

#endif /* _LX_KIT__INTERAL__SLAB_ALLOC_H_ */
