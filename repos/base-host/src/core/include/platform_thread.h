/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
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
	class Platform_thread
	{
		public:

			enum { THREAD_INVALID = -1 };   /* invalid thread number */

			/**
			 * Constructor
			 */
			Platform_thread(const char *name = 0, unsigned priority = 0,
			                addr_t utcb = 0, int thread_id = THREAD_INVALID);

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
			 * Set/get pager
			 */
			void pager(Pager_object *pager) { }
			Pager_object *pager() const { return 0; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() const;

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(unsigned cpu);

			/**
			 * Get thread name
			 */
			const char *name() const { return "noname"; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
