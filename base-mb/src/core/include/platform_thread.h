/*
 * \brief  Thread facility
 * \author Martin Stein
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__INCLUDE__PLATFORM_THREAD_H_
#define _SRC__CORE__INCLUDE__PLATFORM_THREAD_H_

#include <base/pager.h>
#include <base/thread_state.h>
#include <base/thread.h>
#include <base/native_types.h>
#include <kernel/config.h>
#include <util/id_allocator.h>

namespace Genode {

	class Platform_thread;
	typedef Id_allocator<Platform_thread, Native_thread_id, Cpu::BYTE_WIDTH> Tid_allocator;
	Tid_allocator* tid_allocator();

	/**
	 * Base of the physical UTCB belonging to a specific thread
	 */
	Kernel::Utcb* physical_utcb(Native_thread_id tid);


	/**
	 * Set physical UTCB address according to a specific thread ID.
	 * Useful to propagate UTCB address when allocating the whole context
	 * at a stretch as e.g. in core
	 */
	int physical_utcb(Native_thread_id const &tid, Kernel::Utcb * const &utcb);


	class Platform_pd;
	class Platform_thread
	{
		private:

			friend class Platform_pd;

			Native_thread_id  _tid;   /* global kernel thread ID */
			Native_process_id _pid;
			Native_utcb*      _utcb;
			Pager_object     *_pager;
			uint32_t          _params;

			/* for debugging purpose only */
			Platform_pd*      _pd;

			/**
			 * Assign physical thread ID and UTCB address to thread
			 *
			 * This function is called from 'Platform_pd::bind_thread'.
			 */
			void _assign_physical_thread(int pid, Native_utcb* utcb, Platform_pd* pd)
			{
				_utcb = utcb;
				_pid  = pid;
				_pd   = pd;
			}

		public:

			unsigned pid(){ return (unsigned)_pid; }
			unsigned tid(){ return (unsigned)_tid; }

			enum { THREAD_INVALID = -1 };   /* invalid thread number */

			/**
			 * Constructor
			 */
			Platform_thread(const char *name = 0, unsigned priority = 0,
			                int thread_id = THREAD_INVALID, uint32_t params = 0);

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
			 * Request thread state
			 *
			 * \param  state_dst  destination state buffer
			 *
			 * \retval  0 successful
			 * \retval -1 thread state not accessible
			 */
			int state(Genode::Thread_state *state_dst);


			/************************
			 ** Accessor functions **
			 ************************/

			/**
			 * Set pager capability
			 */
			inline Pager_object *pager() { return _pager; }
			inline void pager(Pager_object *pager) { _pager = pager; }

			/**
			 * Return identification of thread when faulting
			 */
		inline unsigned long pager_object_badge() const { return _tid; }

		/**
		 * Set the executing CPU for this thread
		 */
		void affinity(unsigned cpu);

		/**
		 * Get thread name
		 */
		inline const char *name() const { return "noname"; }


		/*********************
		 ** Kernel specific **
		 *********************/

		inline addr_t utcb() const { return (addr_t)_utcb; }
	};
}

#endif /* _SRC__CORE__INCLUDE__PLATFORM_THREAD_H_ */
