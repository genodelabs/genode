/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-10-02
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
#include <thread/capability.h>
#include <base/thread_state.h>
#include <base/thread.h>
#include <nova_native_cpu/nova_native_cpu.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* core includes */
#include <pager.h>

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
				SC_CREATED  = 0x8U,
				REMOTE_PD   = 0x10U,
			};
			uint8_t _features;
			uint8_t _priority;

			Stack::Name _name;

			addr_t _sel_ec()     const { return _id_base; }
			addr_t _sel_pt_oom() const { return _id_base + 1; }
			addr_t _sel_sc()     const { return _id_base + 2; }

			/* convenience function to access _feature variable */
			inline bool main_thread() const { return _features & MAIN_THREAD; }
			inline bool vcpu()        const { return _features & VCPU; }
			inline bool worker()      const { return _features & WORKER; }
			inline bool sc_created()  const { return _features & SC_CREATED; }
			inline bool remote_pd()   const { return _features & REMOTE_PD; }

		public:

			/* mark as vcpu in remote pd if it is a vcpu */
			addr_t remote_vcpu() {
				if (!vcpu())
					return Native_thread::INVALID_INDEX;

				_features |= Platform_thread::REMOTE_PD;
				return _sel_exc_base;
			}

			/* invalid thread number */
			enum { THREAD_INVALID = -1 };

			/**
			 * Constructor
			 */
			Platform_thread(size_t, const char *name = 0,
			                unsigned priority = 0,
			                Affinity::Location affinity = Affinity::Location(),
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
			void pause();

			/**
			 * Enable/disable single stepping
			 */
			void single_step(bool);

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

			/************************
			 ** Accessor functions **
			 ************************/

			/**
			 * Set thread type and exception portal base
			 */
			void thread_type(Nova_native_cpu::Thread_type thread_type,
			                 Nova_native_cpu::Exception_base exception_base);

			/**
			 * Set pager
			 */
			void pager(Pager_object *pager);

			/**
			 * Return pager object
			 */
			Pager_object *pager() { return _pager; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() { return (unsigned long)this; }

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(Affinity::Location location);

			/**
			 * Get the executing CPU for this thread
			 */
			Affinity::Location affinity() const;

			/**
			 * Get thread name
			 */
			const char *name() const { return _name.string(); }

			/**
			 * Get pd name
			 */
			const char *pd_name() const;

			/**
			 * Associate thread with protection domain
			 */
			void bind_to_pd(Platform_pd *pd, bool main_thread)
			{
				_pd = pd;

				if (main_thread) _features |= MAIN_THREAD;
			}

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const quota) { /* not supported*/ }

			/**
			 * Return execution time consumed by the thread
			 */
			unsigned long long execution_time() const;
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
