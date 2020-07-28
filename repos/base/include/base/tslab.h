/*
 * \brief  Typed slab allocator
 * \author Norman Feske
 * \date   2006-05-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__TSLAB_H_
#define _INCLUDE__BASE__TSLAB_H_

#include <base/slab.h>

namespace Genode { template <typename, size_t, unsigned> struct Tslab; }


template <typename T, Genode::size_t BLOCK_SIZE, unsigned MIN_SLABS_PER_BLOCK = 8>
struct Genode::Tslab : Slab
{
	/* check if reasonable amount of entries + overhead fits one block */
	static_assert(MIN_SLABS_PER_BLOCK*(sizeof(T)+overhead_per_entry())
	            + overhead_per_block() <= BLOCK_SIZE);

	Tslab(Allocator *backing_store, void *initial_sb = 0)
	: Slab(sizeof(T), BLOCK_SIZE, initial_sb, backing_store)
	{ }

	Tslab(Allocator &backing_store, void *initial_sb = 0)
	: Slab(sizeof(T), BLOCK_SIZE, initial_sb, &backing_store)
	{ }

	T *first_object() { return (T *)Slab::any_used_elem(); }
};

#endif /* _INCLUDE__BASE__TSLAB_H_ */
