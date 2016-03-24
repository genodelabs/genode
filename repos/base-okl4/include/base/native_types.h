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

	class Platform_thread;

	/**
	 * Index of the UTCB's thread word used for storing the own global
	 * thread ID
	 */
	enum { UTCB_TCR_THREAD_WORD_MYSELF = 0 };

	namespace Thread_id_bits {

		/*
		 * L4 thread ID has 18 bits for thread number and 14 bits for
		 * version info.
		 */
		enum { PD = 8, THREAD = 5 };
	}

	typedef Okl4::L4_ThreadId_t Native_thread_id;

	inline bool operator == (Native_thread_id t1, Native_thread_id t2) {
		return t1.raw == t2.raw; }

	inline bool operator != (Native_thread_id t1, Native_thread_id t2) {
		return t1.raw != t2.raw; }

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

	inline unsigned long convert_native_thread_id_to_badge(Native_thread_id tid)
	{
		/*
		 * OKL4 has no server-defined badges for page-fault messages.
		 * Therefore, we have to interpret the sender ID as badge.
		 */
		return tid.raw;
	}

	struct Cap_dst_policy
	{
		typedef Okl4::L4_ThreadId_t Dst;
		static bool valid(Dst tid) { return !Okl4::L4_IsNilThread(tid); }
		static Dst  invalid()      { return Okl4::L4_nilthread; }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
	typedef Okl4::L4_ThreadId_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
