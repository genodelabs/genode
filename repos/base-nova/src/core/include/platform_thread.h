/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-10-02
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
#include <thread/capability.h>
#include <base/thread_state.h>
#include <base/native_types.h>
#include <base/thread.h>
#include <base/pager.h>

/* core includes */
#include <address_space.h>

namespace Genode {

	class Platform_pd;
	class Platform_thread
	{
		private:

			Platform_pd       *_pd;
			Pager_object      *_pager;
			addr_t             _id_base;
			addr_t             _sel_exc_base;
			Affinity::Location _location;

			enum {
				MAIN_THREAD = 0x1U,
				VCPU        = 0x2U,
				WORKER      = 0x4U,
			};
			uint8_t            _features;
			uint8_t            _priority;

			char               _name[Thread_base::Context::NAME_LEN];

			addr_t _sel_ec() const { return _id_base; }
			addr_t _sel_sc() const { return _id_base + 1; }

			/* convenience function to access _feature variable */
			inline bool is_main_thread() { return _features & MAIN_THREAD; }
			inline bool is_vcpu()        { return _features & VCPU; }
			inline bool is_worker()      { return _features & WORKER; }

		public:

			/* invalid thread number */
			enum { THREAD_INVALID = -1 };

			/**
			 * Constructor
			 */
			Platform_thread(const char *name = 0,
			                unsigned priority = 0,
			                int thread_id = THREAD_INVALID);

			/**
			 * Destructor
			 */
			~Platform_thread();

			/**
			 * Start thread
			 *
			 * \param ip       instruction pointer to start at
			 * \param sp       stack pointer to use
			 *
			 * \retval  0  successful
			 * \retval -1  thread/vCPU could not be started
			 */
			int start(void *ip, void *sp);

			/**
			 * Pause this thread
			 */
			Native_capability pause();

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
			 * Set pager
			 */
			void pager(Pager_object *pager) { _pager = pager; }

			/**
			 * Return pager object
			 */
			Pager_object *pager() { return _pager; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() const;

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(Affinity::Location location);

			/**
			 * Get the executing CPU for this thread
			 */
			Affinity::Location affinity();

			/**
			 * Get thread name
			 */
			const char *name() const { return "noname"; }

			/**
			 * Associate thread with protection domain
			 */
			void bind_to_pd(Platform_pd *pd, bool main_thread)
			{
				_pd = pd;

				if (main_thread) _features |= MAIN_THREAD;
			}

			Native_capability single_step(bool on);

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const quota) { /* not supported*/ }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
