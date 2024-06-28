/*
 * \brief   Pistachio thread facility
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

/* base-internal includes */
#include <base/internal/pistachio.h>

namespace Core {

	class Platform_pd;
	class Platform_thread;
}


inline unsigned long convert_native_thread_id_to_badge(Pistachio::L4_ThreadId_t tid)
{
	/*
	 * Pistachio has no server-defined badges for page-fault messages.
	 * Therefore, we have to interpret the sender ID as badge.
	 */
	return tid.raw;
}


class Core::Platform_thread : Interface
{
	private:

		using L4_ThreadId_t = Pistachio::L4_ThreadId_t;

		/*
		 * Noncopyable
		 */
		Platform_thread(Platform_thread const &);
		Platform_thread &operator = (Platform_thread const &);

		using Name = String<32>;

		Name         const _name;  /* thread name at kernel debugger */
		Platform_pd       &_pd;
		unsigned     const _priority = 0;
		Pager_object      *_pager = nullptr;
		Affinity::Location _location { };

		using Id = Platform_pd::Alloc_thread_id_result;

		Id const _id { _pd.alloc_thread_id(*this) };

		Pistachio::L4_ThreadId_t _l4_id_from_pd_thread_id() const
		{
			using namespace Pistachio;
			return _id.convert<L4_ThreadId_t>(
				[&] (Platform_pd::Thread_id id) { return _pd.l4_thread_id(id); },
				[&] (Platform_pd::Alloc_thread_id_error) { return L4_nilthread; }
			);
		}

		Pistachio::L4_ThreadId_t const _l4_id = _l4_id_from_pd_thread_id();

	public:

		enum { DEFAULT_PRIORITY = 128 };

		/**
		 * Constructor
		 */
		Platform_thread(Platform_pd &pd, size_t, char const *name, unsigned priority,
		                Affinity::Location location, addr_t)
		:
			_name(name), _pd(pd), _priority(priority), _location(location)
		{ }

		/**
		 * Constructor used for initial roottask thread "core.main"
		 */
		Platform_thread(Platform_pd &pd, Pistachio::L4_ThreadId_t l4_id)
		:
			_name("core.main"), _pd(pd), _l4_id(l4_id)
		{ }

		/**
		 * Constructor used for core-internal threads
		 */
		Platform_thread(Platform_pd &pd, char const *name) : _name(name), _pd(pd) { }

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Return true if thread creation suceeded
		 */
		bool valid() const { return _id.ok(); }

		/**
		 * Start thread
		 *
		 * \param ip      instruction pointer to start at
		 * \param sp      stack pointer to use
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
		unsigned long pager_object_badge() const
		{
			return convert_native_thread_id_to_badge(native_thread_id());
		}

		/**
		 * Set the executing CPU for this thread
		 */
		void affinity(Affinity::Location location);

		/**
		 * Request the affinity of this thread
		 */
		Affinity::Location affinity() const;

		/**
		 * Set CPU quota of the thread to 'quota'
		 */
		void quota(size_t const) { /* not supported*/ }

		/**
		 * Return execution time consumed by the thread
		 */
		Trace::Execution_time execution_time() const { return { 0, 0 }; }


		/**********************************
		 ** Pistachio-specific Accessors **
		 **********************************/

		Pistachio::L4_ThreadId_t native_thread_id() const { return _l4_id; }

		Name name() const { return _name; }
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
