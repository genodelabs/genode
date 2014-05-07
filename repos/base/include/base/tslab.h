/*
 * \brief  Typed slab allocator
 * \author Norman Feske
 * \date   2006-05-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__TSLAB_H_
#define _INCLUDE__BASE__TSLAB_H_

#include <base/slab.h>

namespace Genode {

	template <typename T, size_t BLOCK_SIZE>
	class Tslab : public Slab
	{
		public:

			Tslab(Allocator  *backing_store,
			      Slab_block *initial_sb = 0)
			: Slab(sizeof(T), BLOCK_SIZE, initial_sb, backing_store)
			{ }

			T *first_object() { return (T *)Slab::first_used_elem(); }
	};
}

#endif /* _INCLUDE__BASE__TSLAB_H_ */
