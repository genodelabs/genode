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

namespace Genode { template <typename, size_t> struct Tslab; }


template <typename T, Genode::size_t BLOCK_SIZE>
struct Genode::Tslab : Slab
{
	Tslab(Allocator *backing_store, void *initial_sb = 0)
	: Slab(sizeof(T), BLOCK_SIZE, initial_sb, backing_store)
	{ }

	T *first_object() { return (T *)Slab::any_used_elem(); }
};

#endif /* _INCLUDE__BASE__TSLAB_H_ */
