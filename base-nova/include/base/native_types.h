/*
 * \brief  Platform-specific type definitions
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>

namespace Genode {

	typedef volatile int  Native_lock;

	struct Native_thread
	{
		int ec_sel;      /* NOVA cap selector for execution context */
		int sc_sel;      /* NOVA cap selector for scheduling context */
		int rs_sel;      /* NOVA cap selector for running semaphore */
		int pd_sel;      /* NOVA cap selector of protection domain */
		int exc_pt_sel;  /* base of event portal window */
	};

	typedef Native_thread Native_thread_id;

	inline bool operator == (Native_thread_id t1, Native_thread_id t2) { return (t1.ec_sel == t2.ec_sel) && (t1.sc_sel == t2.sc_sel) && (t1.rs_sel == t2.rs_sel); }
	inline bool operator != (Native_thread_id t1, Native_thread_id t2) { return (t1.ec_sel != t2.ec_sel) && (t1.sc_sel != t2.sc_sel) && (t1.rs_sel != t2.rs_sel); }

	class Native_utcb
	{
		private:

			/**
			 * Size of the NOVA-specific user-level thread-control block
			 */
			enum { UTCB_SIZE = 4096 };

			/**
			 * User-level thread control block
			 *
			 * The UTCB is one 4K page, shared between the kernel and the
			 * user process. It is not backed by a dataspace but provided
			 * by the kernel.
			 */
			long _utcb[UTCB_SIZE/sizeof(long)];
	};

	struct Cap_dst_policy
	{
		typedef int Dst;
		static bool valid(Dst pt) { return pt != 0; }
		static Dst  invalid()     { return 0;       }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
