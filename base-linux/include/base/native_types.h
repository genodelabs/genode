/*
 * \brief  Native types
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>
#include <base/stdint.h>

/*
 * We cannot just include <semaphore.h> and <pthread.h> here
 * because this would impy the nested inclusion of a myriad
 * of Linux types and would pollute the namespace for everyone
 * who includes this header file. We want to cleanly separate
 * Genode from POSIX.
 */

namespace Genode {

	/**
	 * Native lock type
	 *
	 * We are using a sleeping spinlock as lock implementation on Linux. This
	 * is a temporary solution until we have implemented futex-based locking.
	 * In a previous version, we have relied on POSIX semaphores as provided by
	 * the glibc. However, relying on the glibc badly interferes with a custom
	 * libc implementation. The glibc semaphore implementation expects to find
	 * a valid pthread structure via the TLS pointer. We do not have such a
	 * structure because we create threads via the 'clone' system call rather
	 * than 'pthread_create'. Hence we have to keep the base framework clean
	 * from glibc usage altogether.
	 */
	typedef volatile int Native_lock;

	/**
	 * Thread ID used in lock implementation
	 *
	 * Unfortunately, both - PID and TID - are needed for lx_tgkill() in
	 * thread_check_stopped_and_restart().
	 */
	struct Native_thread_id
	{
		unsigned int tid;  /* Native thread ID type as returned by the
		                      'clone' system call */
		unsigned int pid;  /* process ID (resp. thread-group ID) */

		Native_thread_id() : tid(0), pid(0) { }
		Native_thread_id(unsigned int tid, unsigned int pid)
		: tid(tid), pid(pid) { }
	};

	struct Thread_meta_data;

	/**
	 * Native thread contains more thread-local data than just the ID
	 *
	 * FIXME doc
	 * A thread needs two sockets as it may be a server that depends on another
	 * service during request processing. If the server socket would be used for
	 * the client call, the server thread may be unblocked by further requests
	 * from its clients. In other words, the additional client socket provides
	 * closed-receive semantics in calls. An even better solution is to use
	 * SCM_RIGHTS messages to send a client socket descriptor with the request.
	 */
	struct Native_thread : Native_thread_id
	{
		int socket;  /* server-entrypoint socket */

		/**
		 * Opaque pointer to additional thread-specific meta data
		 *
		 * This pointer is used by hybrid Linux/Genode program to maintain
		 * POSIX-thread-related meta data. For non-hybrid Genode programs, it
		 * remains unused.
		 */
		Thread_meta_data *meta_data;

		Native_thread() : socket(-1), meta_data(0) { }
	};

	inline bool operator == (Native_thread_id t1, Native_thread_id t2) {
		return (t1.tid == t2.tid) && (t1.pid == t2.pid); }

	inline bool operator != (Native_thread_id t1, Native_thread_id t2) {
		return (t1.tid != t2.tid) || (t1.pid != t2.pid); }

	struct Cap_dst_policy {

		struct Dst
		{
			long tid;  /* XXX to be removed once the transition to SCM rights
			                  is completed */
			int socket;

			Dst() : tid(0), socket(-1) { }

			Dst(long tid, int socket) : tid(tid), socket(socket) { }
		};

		static bool valid(Dst id) { return id.tid != 0; }
		static Dst  invalid()     { return Dst(); }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	/**
	 * Empty UTCB type expected by the thread library, unused on Linux
	 */
	typedef struct { } Native_utcb;

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	class Native_connection_state
	{
		public:

			struct Reply_socket_error { };

		private:

			int _socket;        /* server-entrypoint socket */
			int _reply_socket;  /* reply socket */

		public:

			Native_connection_state() : _socket(-1), _reply_socket(-1) { }

			void socket(int socket) { _socket = socket; }
			int socket() const { return _socket; }

			/*
			 * FIXME Check for unsupported usage pattern: Reply sockets should
			 *       only be set once and read once.
			 */

			void reply_socket(int socket)
			{
				if (_reply_socket != -1) throw Reply_socket_error();

				_reply_socket = socket;
			}

			int reply_socket()
			{
				if (_reply_socket == -1) throw Reply_socket_error();

				int s         = _reply_socket;
				_reply_socket = -1;
				return s;
			}
	};

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 *
		 * Please update platform-specific files after changing these
		 * values, e.g., 'base-linux/src/platform/context_area.*.ld'.
		 */
		static addr_t context_area_virtual_base() { return 0x40000000UL; }
		static addr_t context_area_virtual_size() { return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static addr_t context_virtual_size() { return 0x00100000UL; }
	};
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
