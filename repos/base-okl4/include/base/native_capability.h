/*
 * \brief  Native capability type
 * \author Norman Feske
 * \date   2008-07-26
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_H_

#include <base/native_capability_tpl.h>
#include <base/stdint.h>

namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

namespace Genode {

	struct Cap_dst_policy
	{
		typedef Okl4::L4_ThreadId_t Dst;
		static bool valid(Dst tid) { return !Okl4::L4_IsNilThread(tid); }
		static Dst  invalid()      { return Okl4::L4_nilthread; }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
}

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */
