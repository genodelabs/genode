/*
 * \brief   Fiasco thread facility
 * \author  Christian Helmuth
 * \date    2006-04-11
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
#include <base/native_capability.h>
#include <base/thread_state.h>
#include <base/trace/types.h>

/* core includes */
#include <pager.h>
#include <platform_pd.h>
#include <assertion.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

namespace Core {

	class Platform_pd;
	class Platform_thread;
}


class Core::Platform_thread : Interface
{
	private:

		/*
		 * Noncopyable
		 */
		Platform_thread(Platform_thread const &);
		Platform_thread &operator = (Platform_thread const &);

		using Name = String<32>;

		Name const _name; /* name to be registered at the kernel debugger */

		Platform_pd &_pd;

		using Id = Platform_pd::Alloc_thread_id_result;

		Id const _id { _pd.alloc_thread_id(*this) };

		Pager_object *_pager = nullptr;

	public:

		/**
		 * Constructor
		 */
		Platform_thread(Platform_pd &pd, Rpc_entrypoint &, Ram_allocator &,
		                Region_map &, size_t, const char *name, unsigned,
		                Affinity::Location, addr_t)
		: _name(name), _pd(pd) { }

		/**
		 * Constructor used for core-internal threads
		 */
		Platform_thread(Platform_pd &pd, const char *name)
		: _name(name), _pd(pd) { }

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Return false if thread IDs are exhausted
		 */
		bool valid() const { return _id.ok(); }

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
		void pause() { /* not implemented */ }

		/**
		 * Enable/disable single stepping
		 */
		void single_step(bool) { /* not implemented */ }

		/**
		 * Resume this thread
		 */
		void resume() { /* not implemented */ }

		/**
		 * Override thread state with 's'
		 */
		void state(Thread_state) { /* not implemented */ }

		/**
		 * Read thread state
		 */
		Thread_state state();

		/**
		 * Set the executing CPU for this thread
		 *
		 * SMP is not supported on L4/Fiasco.
		 */
		void affinity(Affinity::Location) { }

		/**
		 * Request the affinity of this thread
		 */
		Affinity::Location affinity() const { return Affinity::Location(); }


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
		 * Return identification of thread when faulting
		 */
		unsigned long pager_object_badge() const {
			return convert_native_thread_id_to_badge(native_thread_id()); }

		/**
		 * Set CPU quota of the thread to 'quota'
		 */
		void quota(size_t) { /* not supported*/ }

		/**
		 * Return execution time consumed by the thread
		 */
		Trace::Execution_time execution_time() const { return { 0, 0 }; }


		/*******************************
		 ** Fiasco-specific Accessors **
		 *******************************/

		Fiasco::l4_threadid_t native_thread_id() const
		{
			using namespace Fiasco;
			return _id.convert<l4_threadid_t>(
				[&] (Platform_pd::Thread_id id) { return _pd.l4_thread_id(id); },
				[&] (Platform_pd::Alloc_thread_id_error) { return L4_INVALID_ID; }
			);
		}

		Name name() const { return _name; }
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
