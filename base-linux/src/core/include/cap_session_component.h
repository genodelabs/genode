/*
 * \brief  Capability allocation service
 * \author Norman Feske
 * \date   2006-06-26
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__LINUX__CAP_SESSION_COMPONENT_H_

#include <cap_session/cap_session.h>
#include <base/rpc_server.h>
#include <base/lock.h>

namespace Genode {

	class Cap_session_component : public Rpc_object<Cap_session>
	{
		private:

			static long _unique_id_cnt;
			static Lock &_lock()
			{
				static Lock static_lock;
				return static_lock;
			}

		public:

			Cap_session_component(Allocator *md_alloc, const char *args) {}

			Native_capability alloc(Native_capability ep)
			{
				Lock::Guard lock_guard(_lock());

				return Native_capability(ep.dst(), ++_unique_id_cnt);
			}

			void free(Native_capability cap) { }
	};
}

#endif /* _CORE__INCLUDE__LINUX__CAP_SESSION_COMPONENT_H_ */
