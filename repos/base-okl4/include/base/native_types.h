/*
 * \brief  Native types on OKL4
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

namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

namespace Genode {

	/**
	 * Index of the UTCB's thread word used for storing the own global
	 * thread ID
	 */
	enum { UTCB_TCR_THREAD_WORD_MYSELF = 0 };


	struct Cap_dst_policy
	{
		typedef Okl4::L4_ThreadId_t Dst;
		static bool valid(Dst tid) { return !Okl4::L4_IsNilThread(tid); }
		static Dst  invalid()      { return Okl4::L4_nilthread; }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
