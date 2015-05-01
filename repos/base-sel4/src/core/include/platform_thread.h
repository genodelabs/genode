/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/pager.h>
#include <base/thread_state.h>
#include <base/native_types.h>

/* core includes */
#include <address_space.h>

namespace Genode {

	class Platform_pd;
	class Platform_thread;
}



class Genode::Platform_thread
{
	private:

		Pager_object *_pager = nullptr;

		Weak_ptr<Address_space> _address_space;

		friend class Platform_pd;

	public:

		/**
		 * Constructor
		 */
		Platform_thread(size_t, const char *name = 0, unsigned priority = 0,
		                addr_t utcb = 0);

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Start thread
		 *
		 * \param ip      instruction pointer to start at
		 * \param sp      stack pointer to use
		 * \param cpu_no  target cpu
		 *
		 * \retval  0  successful
		 * \retval -1  thread could not be started
		 */
		int start(void *ip, void *sp, unsigned int cpu_no = 0);

		/**
		 * Pause this thread
		 */
		void pause();

		/**
		 * Resume this thread
		 */
		void resume();

		/**
		 * Cancel currently blocking operation
		 */
		void cancel_blocking();

		/**
		 * Override thread state with 's'
		 *
		 * \throw Cpu_session::State_access_failed
		 */
		void state(Thread_state s);

		/**
		 * Read thread state
		 *
		 * \throw Cpu_session::State_access_failed
		 */
		Thread_state state();

		/**
		 * Return the address space to which the thread is bound
		 */
		Weak_ptr<Address_space> address_space();


		/************************
		 ** Accessor functions **
		 ************************/

		/**
		 * Set pager capability
		 */
		Pager_object *pager(Pager_object *pager) const { return _pager; }
		void pager(Pager_object *pager) { _pager = pager; }
		Pager_object *pager() { return _pager; }

		/**
		 * Return identification of thread when faulting
		 */
		unsigned long pager_object_badge() const
		{
			PDBG("not implemented");
			return 0;
		}

		/**
		 * Set the executing CPU for this thread
		 */
		void affinity(Affinity::Location) { }

		/**
		 * Get the executing CPU for this thread
		 */
		Affinity::Location affinity() { return Affinity::Location(); }

		/**
		 * Set CPU quota of the thread
		 */
		void quota(size_t) { /* not supported */ }

		/**
		 * Get thread name
		 */
		const char *name() const { return "noname"; }
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
