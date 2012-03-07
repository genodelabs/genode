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

#include <util/string.h>

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

	/*
	 * Because Codezero does not support local names for capabilities, a Genode
	 * capability consists of the global thread ID and a global object ID, not
	 * protected by the kernel when transmitted as IPC payloads.
	 */
	class Native_capability
	{
		private:

			Native_thread_id _tid;  /* global thread ID */
			int _local_name;        /* global unique object ID */

		protected:

			Native_capability(void* ptr) : _local_name((int)ptr) {
				_tid.tid = Codezero::NILTHREAD; }

		public:

			/**
			 * Default constructor creates invalid capability
			 */
			Native_capability()
			: _local_name(0) { _tid.tid = Codezero::NILTHREAD; }

			/**
			 * Constructor for hand-crafting capabilities
			 *
			 * This constructor is only used internally be the framework.
			 */
			Native_capability(Native_thread_id tid, int local_name)
			: _tid(tid), _local_name(local_name) { }

			bool valid() const { return _tid.tid != Codezero::NILTHREAD; }

			int local_name() const { return _local_name; }
			void* local() const { return (void*)_local_name;  }
			int dst() const { return _tid.tid; }

			Native_thread_id tid() const { return _tid; }

			/**
			 * Copy this capability to another pd.
			 */
			void copy_to(void* dst) {
				memcpy(dst, this, sizeof(Native_capability)); }
	};

	typedef int Native_connection_state;
}


#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
