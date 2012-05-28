/*
 * \brief  Thread facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
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

namespace Genode {

	class Platform_pd;
	class Platform_thread
	{
		private:

			Platform_pd  *_pd;
			Pager_object *_pager;
			bool          _is_main_thread;
			int           _id;

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
			 * Set pager
			 */
			void pager(Pager_object *pager) { _pager = pager; }

			Pager_object *pager() { return _pager; }

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() const;

			/**
			 * Set the executing CPU for this thread.
			 */
			void set_cpu(unsigned int cpu_no);

			/**
			 * Get thread name
			 */
			const char *name() const { return "noname"; }

			/**
			 * Associate thread with protection domain
			 */
			void bind_to_pd(Platform_pd *pd, bool is_main_thread)
			{
				_pd = pd, _is_main_thread = is_main_thread;
			}
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
