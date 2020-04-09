/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_

#include <base/stdint.h>
#include <base/native_capability.h>

#include <linux_syscalls.h>

namespace Genode { struct Native_thread; }

class Genode::Native_thread
{
	private:

		/*
		 * Noncopyable
		 */
		Native_thread(Native_thread const &);
		Native_thread &operator = (Native_thread const &);

	public:

		/*
		 * Unfortunately, both - PID and TID - are needed for lx_tgkill()
		 */
		unsigned int tid = 0;  /* Native thread ID type as returned by the
		                          'clone' system call */
		unsigned int pid = 0;  /* process ID (resp. thread-group ID) */

		bool is_ipc_server = false;

		/**
		 * Natively aligned memory location used in the lock implementation
		 */
		int futex_counter __attribute__((aligned(sizeof(Genode::addr_t)))) = 0;

		struct Meta_data;

		/**
		 * Opaque pointer to additional thread-specific meta data
		 *
		 * This pointer is used by hybrid Linux/Genode programs to maintain
		 * POSIX-thread-related meta data. For non-hybrid Genode programs, it
		 * remains unused.
		 */
		Meta_data *meta_data = nullptr;

		class Epoll
		{
			private:

				Lx_socketpair _control { };

				Lx_epoll_sd const _epoll;

				void _add   (Lx_sd);
				void _remove(Lx_sd);

				bool _rpc_ep_exited = false;

				struct Control_function : Interface
				{
					virtual void execute() = 0;
				};

				/*
				 * Execute functor 'fn' in the context of the 'poll' method.
				 */
				template <typename FN>
				void _exec_control(FN const &);

			public:

				Epoll();

				~Epoll();

				/**
				 * Wait for incoming RPC messages
				 *
				 * \return  valid socket descriptor that matches the invoked
				 *          RPC object
				 */
				Lx_sd poll();

				Native_capability alloc_rpc_cap();

				void free_rpc_cap(Native_capability);

				/**
				 * Flag RPC entrypoint as no longer in charge of dispatching
				 */
				void rpc_ep_exited() { _rpc_ep_exited = true; }

		} epoll { };

		Native_thread() { }
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */
