/*
 * \brief  Native types on L4/Fiasco
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

/* Genode includes */
#include <base/native_capability.h>
#include <base/stdint.h>

namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Platform_thread;

	struct Cap_dst_policy
	{
		typedef Fiasco::l4_threadid_t Dst;
		static bool valid(Dst id) { return !Fiasco::l4_is_invalid_id(id); }
		static Dst invalid()
		{
			using namespace Fiasco;
			return L4_INVALID_ID;
		}
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
