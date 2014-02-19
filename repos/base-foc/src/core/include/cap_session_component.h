/*
 * \brief  Capability session service
 * \author Stefan Kalkowski
 * \date   2011-01-13
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/allocator.h>
#include <base/object_pool.h>

namespace Genode {

	class Cap_session_component : public Rpc_object<Cap_session>
	{
		private:

			struct Entry : Object_pool<Entry>::Entry
			{
				Entry(Native_capability cap) : Object_pool<Entry>::Entry(cap) {}
			};

			Object_pool<Entry> _pool;
			Allocator         *_md_alloc;

		public:

			Cap_session_component(Allocator *md_alloc, const char *args)
			: _md_alloc(md_alloc) {}

			~Cap_session_component();

			void upgrade_ram_quota(size_t ram_quota) { }

			Native_capability alloc(Native_capability ep);

			void free(Native_capability cap);
	};
}

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
