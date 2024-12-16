/*
 * \brief  Linux thread facility
 * \author Norman Feske
 * \date   2006-06-13
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
#include <base/registry.h>
#include <base/weak_ptr.h>
#include <cpu_session/cpu_session.h>

/* core includes */
#include <pager.h>

namespace Core {
	class Platform_thread;
	class Platform_pd;
}


/*
 * We hold all Platform_thread objects in a list in order to be able to
 * reflect SIGCHLD as exception signals. When a SIGCHILD occurs, we
 * determine the PID of the terminated child process via 'wait4'. We use
 * the list to find the 'Platform_thread' matching the TID, wherei, in
 * turn, we find the exception handler's 'Signal_context_capability'.
 */

class Core::Platform_thread : Noncopyable
{
	using Location = Affinity::Location;
	using Execution_time = Trace::Execution_time;

	private:

		unsigned long _tid = -1;
		unsigned long _pid = -1;

		String<32> const _name;

		/*
		 * Dummy pager object that is solely used for storing the
		 * 'Signal_context_capability' for the thread's exception handler.
		 */
		Pager_object _pager { };

		/**
		 * Singleton instance of platform-thread registry used to deliver
		 * exceptions.
		 */
		static Registry<Platform_thread> &_registry();

		Registry<Platform_thread>::Element _elem { _registry(), *this };

	public:

		/**
		 * Constructor
		 */
		Platform_thread(Platform_pd &, Rpc_entrypoint &, Ram_allocator &, Region_map &,
		                size_t, auto const &name, auto...)
		: _name(name) { }

		/**
		 * Return true if thread creation succeeded
		 */
		bool valid() const { return true; }

		const char *name() { return _name.string(); }

		/**
		 * Notify Genode::Signal handler about sigchld
		 */
		static void submit_exception(unsigned pid);

		/**
		 * Register process ID and thread ID of thread
		 */
		void thread_id(int pid, int tid) { _pid = pid, _tid = tid; }

		/*
		 * Part of the platform-thread interface that is not used on Linux
		 */

		void           pause()                    { };
		void           single_step(bool)          { }
		void           resume()                   { }
		Pager_object  &pager()                    { return _pager; }
		void           pager(Pager_object &)      { }
		void           start(void *, void *)      { }
		void           affinity(Location)         { }
		Location       affinity() const           { return { }; }
		void           quota(size_t)              { }
		void           state(Thread_state)        { }
		Execution_time execution_time() const     { return { 0, 0 }; }
		unsigned long  pager_object_badge() const { return 0; }

		Thread_state state()
		{
			return { .state = Thread_state::State::UNAVAILABLE, .cpu = { } };
		}
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
