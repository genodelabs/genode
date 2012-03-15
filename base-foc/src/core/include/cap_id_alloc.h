/*
 * \brief  Capability IDs allocation service
 * \author Stefan Kalkowski
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_IP_ALLOC_H_
#define _CORE__INCLUDE__CAP_IP_ALLOC_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/lock.h>
#include <base/sync_allocator.h>

namespace Genode {

	class Cap_id_allocator
	{
		private:

			enum {
				CAP_ID_RANGE   = ~0UL,
				CAP_ID_MASK    = ~3UL,
				CAP_ID_NUM_MAX = CAP_ID_MASK >> 2,
				CAP_ID_OFFSET  = 1 << 2
			};

			Synchronized_range_allocator<Allocator_avl> _id_alloc;
			Lock                                        _lock;

		public:

			class Out_of_ids : Exception {};


			Cap_id_allocator(Allocator*);

			unsigned long alloc();
			void free(unsigned long id);
	};
}

#endif /* _CORE__INCLUDE__CAP_IP_ALLOC_H_ */
