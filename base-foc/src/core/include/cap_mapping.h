/*
 * \brief   Fiasco.OC specific capability mapping.
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_MAPPING_H_
#define _CORE__INCLUDE__CAP_MAPPING_H_

/* core includes */
#include <cap_index.h>
#include <util/noncopyable.h>

namespace Genode {

	/**
	 * A Cap_mapping embodies a capability of core, and its mapped
	 * copy in another protection domain.
	 */
	class Cap_mapping : Noncopyable
	{
		private:

			/**
			 * Helper function for construction purposes.
			 *
			 * Allocates a new capability id and Core_cap_index and inserts
			 * it in the Cap_map.
			 *
			 * \return pointer to newly constructed Core_cap_index object
			 */
			inline Core_cap_index* _get_cap();

		public:

			Native_capability local;  /* reference to cap that is mapped */
			Native_thread_id  remote; /* index in cap-space of the other pd */

			Cap_mapping(bool alloc=false,
			            Native_thread_id r = Fiasco::L4_INVALID_CAP);
			Cap_mapping(Native_capability cap,
			            Native_thread_id r = Fiasco::L4_INVALID_CAP);

			/**
			 * Map the cap in local to corresponding task.
			 *
			 * \param task capability of task to map to
			 */
			void map(Native_task task);
	};
}

#endif /* _CORE__INCLUDE__CAP_MAPPING_H_ */
