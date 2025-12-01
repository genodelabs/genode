/*
 * \brief  Page-granular allocator for core-private RAM
 * \author Norman Feske
 * \date   2025-09-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__MAPPED_RAM_H_
#define _CORE__INCLUDE__MAPPED_RAM_H_

/* core includes */
#include <types.h>

namespace Core { struct Mapped_ram_allocator; }


struct Core::Mapped_ram_allocator
{
	struct Attr
	{
		void * _ptr;
		size_t _num_bytes;

		size_t num_bytes() const { return _num_bytes; }
		void * ptr()             { return _ptr; }
	};

	enum class Error { DENIED };

	using Allocation = Genode::Allocation<Mapped_ram_allocator>;
	using Result     = Allocation::Attempt;

	Result alloc(size_t, Align);

	void _free(Allocation &);
};

#endif /* _CORE__INCLUDE__MAPPED_RAM_H_ */
