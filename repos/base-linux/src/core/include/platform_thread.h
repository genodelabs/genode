/*
 * \brief  Linux thread facility
 * \author Norman Feske
 * \date   2006-06-13
 *
 * Pretty dumb.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/thread_state.h>
#include <base/trace/types.h>
#include <base/weak_ptr.h>
#include <cpu_session/cpu_session.h>

/* core includes */
#include <pager.h>

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
				Lock                  _lock { };
				List<Platform_thread> _list { };

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
			static Registry &_registry();

			unsigned long _tid = -1;
			unsigned long _pid = -1;
			char          _name[32] { };

			/*
			 * Dummy pager object that is solely used for storing the
			 * 'Signal_context_capability' for the thread's exception handler.
			 */
			Pager_object _pager { };

		public:

			/**
			 * Constructor
			 */
			Platform_thread(size_t, const char *name, unsigned priority,
			                Affinity::Location, addr_t);

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
			 * Enable/disable single stepping
			 */
			void single_step(bool) { }

			/**
			 * Resume this thread
			 */
			void resume();

			/**
			 * Dummy implementation of platform-thread interface
			 */
			Pager_object &pager() { return _pager; }
			void          pager(Pager_object &) { }
			int           start(void *, void *) { return 0; }

			Thread_state state()
			{
				warning("Not implemented");
				throw Cpu_thread::State_access_failed();
			}

			void state(Thread_state)
			{
				warning("Not implemented");
				throw Cpu_thread::State_access_failed();
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
			Affinity::Location affinity() const { return Affinity::Location(); }

			/**
			 * Register process ID and thread ID of thread
			 */
			void thread_id(int pid, int tid) { _pid = pid, _tid = tid; }

			/**
			 * Notify Genode::Signal handler about sigchld
			 */
			static void submit_exception(int pid)
			{
				_registry().submit_exception(pid);
			}

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const) { /* not supported*/ }

			/**
			 * Return execution time consumed by the thread
			 */
			Trace::Execution_time execution_time() const { return { 0, 0 }; }

			unsigned long pager_object_badge() const { return 0; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
