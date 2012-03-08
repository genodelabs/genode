/*
 * \brief  Dummy definitions for native types used for compiling unit tests
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

namespace Codezero {

	struct l4_mutex;

	enum { NILTHREAD = -1 };
}

namespace Genode {

	class Platform_thread;

	struct Native_thread_id
	{
		int tid;

		/**
		 * Pointer to thread's running lock
		 *
		 * Once initialized (see 'lock_helper.h'), it will point to the
		 * '_running_lock' field of the thread's 'Native_thread' structure,
		 * which is part of the thread context. This member variable is
		 * used by the lock implementation only.
		 */
		struct Codezero::l4_mutex *running_lock;

		Native_thread_id() { }

		/**
		 * Constructor (used as implicit constructor)
		 */
		Native_thread_id(int l4id) : tid(l4id), running_lock(0) { }

		Native_thread_id(int l4id, Codezero::l4_mutex *rl) : tid(l4id), running_lock(rl) { }

		static bool valid(int tid) { return tid != Codezero::NILTHREAD; }
		static int  invalid()      { return Codezero::NILTHREAD;        }
	};

	struct Native_thread
	{
		Native_thread_id l4id;

		/**
		 * Only used in core
		 *
		 * For 'Thread' objects created within core, 'pt' points to the
		 * physical thread object, which is going to be destroyed on
		 * destruction of the 'Thread'.
		 */
		Platform_thread *pt;
	};

	/**
	 * Empty UTCB type expected by the thread library
	 *
	 * On this kernel, UTCBs are not placed within the the context area. Each
	 * thread can request its own UTCB pointer using the kernel interface.
	 * However, we use the 'Native_utcb' member of the thread context to
	 * hold thread-specific data, i.e. the running lock used by the lock
	 * implementation.
	 */
	struct Native_utcb
	{
		private:

			/**
			 * Prevent construction
			 *
			 * A UTCB is never constructed, it is backed by zero-initialized memory.
			 */
			Native_utcb();

			/**
			 * Backing store for per-thread running lock
			 *
			 * The size of this member must equal 'sizeof(Codezero::l4_mutex)'.
			 * Unfortunately, we cannot include the Codezero headers here.
			 */
			int _running_lock;

		public:

			Codezero::l4_mutex *running_lock() {
				return (Codezero::l4_mutex *)&_running_lock; }
	};

	inline bool operator == (Native_thread_id t1, Native_thread_id t2) { return t1.tid == t2.tid; }
	inline bool operator != (Native_thread_id t1, Native_thread_id t2) { return t1.tid != t2.tid; }

	typedef Native_capability_tpl<int,Native_thread_id> Native_capability;
	typedef int Native_connection_state;
}


#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
