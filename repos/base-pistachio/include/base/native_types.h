/*
 * \brief  Native types on Pistachio
 * \author Norman Feske
 * \date   2008-07-26
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>
#include <base/stdint.h>

namespace Pistachio {
#include <l4/types.h>
}

namespace Genode {

	class Platform_thread;

	typedef Pistachio::L4_ThreadId_t Native_thread_id;

	struct Cap_dst_policy
	{
		typedef Pistachio::L4_ThreadId_t Dst;
		static bool valid(Dst tid) { return !Pistachio::L4_IsNilThread(tid); }
		static Dst  invalid()
		{
			using namespace Pistachio;
			return L4_nilthread;
		}
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	struct Native_thread
	{
		Native_thread_id l4id;

		/**
		 * Only used in core
		 *
		 * For 'Thread' objects created within core, 'pt' points to
		 * the physical thread object, which is going to be destroyed
		 * on destruction of the 'Thread'.
		 */
		Platform_thread *pt;
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	typedef Pistachio::L4_ThreadId_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
