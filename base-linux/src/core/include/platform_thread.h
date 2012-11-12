/*
 * \brief  Linux thread facility
 * \author Norman Feske
 * \date   2006-06-13
 *
 * Pretty dumb.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_

#include <base/pager.h>
#include <base/thread_state.h>
#include <cpu_session/cpu_session.h>

namespace Genode {

	class Platform_thread
	{
		private:

			unsigned long _tid;
			unsigned long _pid;
			char          _name[32];

			/**
			 * Unix-domain socket pair bound to the thread
			 */
			Native_connection_state _ncs;

		public:

			/**
			 * Constructor
			 */
			Platform_thread(const char *name, unsigned priority, addr_t);

			~Platform_thread();

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking();

			/**
			 * Pause this thread
			 */
			void pause();

			/**
			 * Resume this thread
			 */
			void resume();

			/**
			 * Dummy implementation of platform-thread interface
			 */
			Pager_object *pager() { return 0; }
			void          pager(Pager_object *) { }
			int           start(void *ip, void *sp) { return 0; }

			Thread_state state()
			{
				PDBG("Not implemented");
				throw Cpu_session::State_access_failed();
			}

			void state(Thread_state)
			{
				PDBG("Not implemented");
				throw Cpu_session::State_access_failed();
			}

			const char   *name() { return _name; }
			void          affinity(unsigned) { }

			/**
			 * Register process ID and thread ID of thread
			 */
			void thread_id(int pid, int tid) { _pid = pid, _tid = tid; }

			/**
			 * Return client-side socket descriptor
			 *
			 * For more information, please refer to the comments in
			 * 'linux_cpu_session/linux_cpu_session.h'.
			 */
			int client_sd();

			/**
			 * Return server-side socket descriptor
			 */
			int server_sd();
	};
}

#endif /* _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_ */
