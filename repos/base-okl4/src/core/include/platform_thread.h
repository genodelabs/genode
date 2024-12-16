/*
 * \brief   OKL4 thread facility
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/thread_state.h>
#include <base/trace/types.h>

/* core includes */
#include <pager.h>
#include <platform_pd.h>
#include <assertion.h>

namespace Core {

	class Platform_pd;
	class Platform_thread;
}


class Core::Platform_thread
{
	private:

		/*
		 * Noncopyable
		 */
		Platform_thread(Platform_thread const &);
		Platform_thread &operator = (Platform_thread const &);

		int _thread_id = THREAD_INVALID;    /* plain thread number */

		Okl4::L4_ThreadId_t _l4_thread_id = Okl4::L4_nilthread;

		char          _name[32];            /* thread name that will be
		                                       registered at the kernel
		                                       debugger */
		Platform_pd  &_pd;
		unsigned      _priority = 0;        /* thread priority */
		Pager_object *_pager = nullptr;

		bool _bound_to_pd = false;

	public:

		enum { THREAD_INVALID   = -1 };   /* invalid thread number */
		enum { DEFAULT_PRIORITY = 128 };

		/**
		 * Constructor
		 */
		Platform_thread(Platform_pd &pd, Rpc_entrypoint &, Ram_allocator &,
		                Region_map &, size_t, const char *name,
		                unsigned prio, Affinity::Location, addr_t)
		:
			_pd(pd), _priority(prio)
		{
			copy_cstring(_name, name, sizeof(_name));
			_bound_to_pd = pd.bind_thread(*this);
		}

		/**
		 * Constructor used for core-internal threads
		 */
		Platform_thread(Platform_pd &pd, char const *name) : _pd(pd)
		{
			copy_cstring(_name, name, sizeof(_name));
			_bound_to_pd = pd.bind_thread(*this);
		}

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Return true if thread creation succeeded
		 */
		bool valid() const { return _bound_to_pd; }

		/**
		 * Start thread
		 *
		 * \param ip  instruction pointer to start at
		 * \param sp  stack pointer to use
		 */
		void start(void *ip, void *sp);

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
		 * This thread is about to be bound
		 *
		 * \param thread_id     local thread ID
		 * \param l4_thread_id  final L4 thread ID
		 */
		void bind(int thread_id, Okl4::L4_ThreadId_t l4_thread_id);

		/**
		 * Unbind this thread
		 */
		void unbind();

		/**
		 * Override thread state with 's'
		 */
		void state(Thread_state s);

		/**
		 * Read thread state
		 */
		Thread_state state();

		/************************
		 ** Accessor functions **
		 ************************/

		/**
		 * Return/set pager
		 */
		Pager_object &pager() const
		{
			if (_pager)
				return *_pager;

			ASSERT_NEVER_CALLED;
		}

		void pager(Pager_object &pager) { _pager = &pager; }

		/**
		 * Get the 'Platform_pd' object this thread belongs to
		 */
		Platform_pd &pd() { return _pd; }

		/**
		 * Return identification of thread when faulting
		 */
		unsigned long pager_object_badge() const;

		/**
		 * Set the executing CPU for this thread
		 */
		void affinity(Affinity::Location) { }

		/**
		 * Request the affinity of this thread
		 */
		Affinity::Location affinity() const { return Affinity::Location(); }

		/**
		 * Set CPU quota of the thread
		 */
		void quota(size_t) { /* not supported */ }

		/**
		 * Return execution time consumed by the thread
		 */
		Trace::Execution_time execution_time() const { return { 0, 0 }; }


		/*****************************
		 ** OKL4-specific Accessors **
		 *****************************/

		int                 thread_id()        const { return _thread_id; }
		Okl4::L4_ThreadId_t native_thread_id() const { return _l4_thread_id; }
		const char         *name()             const { return _name; }

		void set_l4_thread_id(Okl4::L4_ThreadId_t id) { _l4_thread_id = id; }
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
