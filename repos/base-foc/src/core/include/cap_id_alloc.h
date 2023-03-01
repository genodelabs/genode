/*
 * \brief  Capability IDs allocation service
 * \author Stefan Kalkowski
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CAP_ID_ALLOC_H_
#define _CORE__INCLUDE__CAP_ID_ALLOC_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/mutex.h>
#include <synced_range_allocator.h>

/* core includes */
#include <types.h>

namespace Core { class Cap_id_allocator; }


class Core::Cap_id_allocator
{
	public:

		using id_t = uint16_t;

		enum { ID_MASK = 0xffff };

	private:

		enum {
			CAP_ID_RANGE   = ~0UL,
			CAP_ID_MASK    = ~3UL,
			CAP_ID_NUM_MAX = CAP_ID_MASK >> 2,
			CAP_ID_OFFSET  = 1 << 2
		};

		Synced_range_allocator<Allocator_avl> _id_alloc;

		Mutex _mutex { };

	public:

		class Out_of_ids : Exception {};

		Cap_id_allocator(Allocator &);

		id_t alloc();
		void free(id_t id);
};

#endif /* _CORE__INCLUDE__CAP_ID_ALLOC_H_ */
