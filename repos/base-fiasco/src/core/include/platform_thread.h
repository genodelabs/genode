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

/* core includes */
#include <pager.h>
#include <platform_pd.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Platform_pd;
	class Platform_thread : Interface
	{
		private:

			/*
			 * Noncopyable
			 */
			Platform_thread(Platform_thread const &);
			Platform_thread &operator = (Platform_thread const &);

			int                   _thread_id;      /* plain thread number */
			Fiasco::l4_threadid_t _l4_thread_id;   /* L4 thread ID */
			char                  _name[32];       /* thread name that will be
			                                          registered at the kernel
			                                          debugger */
			Platform_pd *_platform_pd = nullptr;   /* protection domain thread
			                                          is bound to */
			Pager_object *_pager;

		public:

			enum {
				THREAD_INVALID = -1,  /* invalid thread number */
			};

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
			 * \param ip  instruction pointer to start at
			 * \param sp  stack pointer to use
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
			void bind(int thread_id, Fiasco::l4_threadid_t l4_thread_id,
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
			 *
			 * \throw Cpu_session::State_access_failed
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
			Pager_object *pager() const { return _pager; }
			void pager(Pager_object *pager) { _pager = pager; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() const {
				return convert_native_thread_id_to_badge(_l4_thread_id); }

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t) { /* not supported*/ }

			/**
			 * Return execution time consumed by the thread
			 */
			unsigned long long execution_time() const { return 0; }


			/*******************************
			 ** Fiasco-specific Accessors **
			 *******************************/

			int                   thread_id()        const { return _thread_id; }
			Fiasco::l4_threadid_t native_thread_id() const { return _l4_thread_id; }
			const char           *name()             const { return _name; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
