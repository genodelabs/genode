/*
 * \brief  Linux thread facility
 * \author Norman Feske
 * \date   2006-06-13
 *
 * Pretty dumb.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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

	class Platform_thread;

	/*
	 * We hold all Platform_thread objects in a list in order to be able to
	 * reflect SIGCHLD as exception signals. When a SIGCHILD occurs, we
	 * determine the PID of the terminated child process via 'wait4'. We use
	 * the list to find the 'Platform_thread' matching the TID, wherei, in
	 * turn, we find the exception handler's 'Signal_context_capability'.
	 */

	class Platform_thread : public List<Platform_thread>::Element
	{
		private:

			struct Registry
			{
				Lock                  _lock;
				List<Platform_thread> _list;

				void insert(Platform_thread *thread);
				void remove(Platform_thread *thread);

				/**
				 * Trigger exception handler for 'Platform_thread' with matching PID.
				 */
				void submit_exception(unsigned long pid);
			};

			/**
			 * Return singleton instance of 'Platform_thread::Registry'
			 */
			static Registry *_registry();

			unsigned long _tid;
			unsigned long _pid;
			char          _name[32];

			/**
			 * Unix-domain socket pair bound to the thread
			 */
			Native_connection_state _ncs;

			/*
			 * Dummy pager object that is solely used for storing the
			 * 'Signal_context_capability' for the thread's exception handler.
			 */
			Pager_object _pager;

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
			Pager_object *pager() { return &_pager; }
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

			/**
			 * Set the executing CPU for this thread
			 *
			 * SMP is currently not directly supported on Genode/Linux
			 * (but indirectly by the Linux kernel).
			 */
			void affinity(Affinity::Location) { }

			/**
			 * Request the affinity of this thread
			 */
			Affinity::Location affinity() { return Affinity::Location(); }

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

			/**
			 * Notify Genode::Signal handler about sigchld
			 */
			static void submit_exception(int pid)
			{
				_registry()->submit_exception(pid);
			}

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const quota) { /* not supported*/ }
	};
}

#endif /* _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_ */
