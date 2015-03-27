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
		private:

			friend class Platform_pd;

			enum { PD_NAME_MAX_LEN = 64 };

			unsigned                _tid;   /* global codezero thread ID */
			unsigned                _space_id;
			Weak_ptr<Address_space> _address_space;
			addr_t                  _utcb;
			char                    _name[PD_NAME_MAX_LEN];
			Pager_object           *_pager;

			/**
			 * Assign physical thread ID and UTCB address to thread
			 *
			 * This function is called from 'Platform_pd::bind_thread'.
			 */
			void _assign_physical_thread(unsigned tid, unsigned space_id,
			                             addr_t utcb,
			                             Weak_ptr<Address_space> address_space)
			{
				_tid = tid; _space_id = space_id; _utcb = utcb;
				_address_space = address_space;
			}

		public:

			enum { THREAD_INVALID = -1 };   /* invalid thread number */

			/**
			 * Constructor
			 */
			Platform_thread(size_t, const char *name = 0, unsigned priority = 0,
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
			 * Set pager capability
			 */
			Pager_object *pager(Pager_object *pager) const { return _pager; }
			void pager(Pager_object *pager) { _pager = pager; }
			Pager_object *pager() { return _pager; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() const { return _tid; }

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(Affinity::Location) { }

			/**
			 * Get the executing CPU for this thread
			 */
			Affinity::Location affinity() { return Affinity::Location(); }

			/**
			 * Get thread name
			 */
			const char *name() const { return "noname"; }


			/***********************
			 ** Codezero specific **
			 ***********************/

			addr_t utcb() const { return _utcb; }

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const quota) { /* not supported*/ }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
