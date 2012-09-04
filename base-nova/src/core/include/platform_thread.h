/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Alexander Boettcher
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
			addr_t        _id_base;
			addr_t        _sel_exc_base;
			unsigned      _cpu_no;
			bool          _is_main_thread;
			bool          _is_vcpu;

			addr_t _sel_ec()       { return _id_base; }
			addr_t _sel_sc()       { return _id_base + 1; }

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
			void affinity(unsigned cpu);

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

			/**
			 * Return native EC cap with specific rights mask set.
			 * If the cap is mapped the kernel will demote the
			 * rights of the EC as specified by the rights mask.
			 *
			 * The cap is supposed to be returned to clients,
			 * which they have to use as argument to identify
			 * the thread to which they want attach portals.
			 *
			 * The demotion by the kernel during the map operation
			 * takes care that the EC cap itself contains
			 * no usable rights for the clients.
			 */
			Native_capability native_cap()
			{
				using namespace Nova;

				return Native_capability(
					_sel_ec(), Obj_crd::RIGHT_EC_RECALL);
			}

			void single_step(bool on);

	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
