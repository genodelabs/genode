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

/* core includes */
#include <pager.h>
#include <platform_pd.h>
#include <address_space.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/types.h>
}


inline unsigned long convert_native_thread_id_to_badge(Pistachio::L4_ThreadId_t tid)
{
	/*
	 * Pistachio has no server-defined badges for page-fault messages.
	 * Therefore, we have to interpret the sender ID as badge.
	 */
	return tid.raw;
}


namespace Genode {

	class Platform_pd;
	class Platform_thread
	{
		private:

			int                      _thread_id;      /* plain thread number */
			Pistachio::L4_ThreadId_t _l4_thread_id;   /* L4 thread ID */
			char                     _name[32];       /* thread name that will be
			                                             registered at the kernel
			                                             debugger */
			Platform_pd             *_platform_pd;    /* protection domain thread
			                                             is bound to */
			unsigned                 _priority;       /* thread priority */
			Pager_object            *_pager;

			Affinity::Location       _location;

		public:

			enum { THREAD_INVALID = -1 };       /* invalid thread number */
			enum { DEFAULT_PRIORITY = 128 };

			/**
			 * Constructor
			 */
			Platform_thread(size_t, const char *name = 0, unsigned priority = 0,
			                Affinity::Location = Affinity::Location(),
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
			 *
			 * \retval  0  successful
			 * \retval -1  thread could not be started
			 */
			int start(void *ip, void *sp);

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
			 * Cancel currently blocking operation
			 */
			void cancel_blocking();

			/**
			 * This thread is about to be bound
			 *
			 * \param thread_id     local thread ID
			 * \param l4_thread_id  final L4 thread ID
			 * \param pd            platform pd, thread is bound to
			 */
			void bind(int thread_id, Pistachio::L4_ThreadId_t l4_thread_id,
			          Platform_pd *pd);

			/**
			 * Unbind this thread
			 */
			void unbind();

			/**
			 * Override thread state with 's'
			 *
			 * \throw Cpu_session::State_access_failed
			 */
			void state(Thread_state s);

			/**
			 * Read thread state
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
			 * Return/set pager
			 */
			Pager_object *pager() const { return _pager; }
			void pager(Pager_object *pager) { _pager = pager; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() const {
				return convert_native_thread_id_to_badge(_l4_thread_id); }

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
			void quota(size_t const quota) { /* not supported*/ }

			/**
			 * Return execution time consumed by the thread
			 */
			unsigned long long execution_time() const { return 0; }


			/**********************************
			 ** Pistachio-specific Accessors **
			 **********************************/

			int                      thread_id()        const { return _thread_id; }
			Pistachio::L4_ThreadId_t native_thread_id() const { return _l4_thread_id; }
			const char              *name()             const { return _name; }

			/* use only for core... */
			void set_l4_thread_id(Pistachio::L4_ThreadId_t id) { _l4_thread_id = id; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
