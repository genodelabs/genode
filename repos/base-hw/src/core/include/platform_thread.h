/*
 * \brief   Userland interface for the management of kernel thread-objects
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <ram_session/ram_session.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>

/* core includes */
#include <address_space.h>
#include <object.h>

/* kernel includes */
#include <kernel/core_interface.h>
#include <kernel/thread.h>

namespace Genode {

	class Pager_object;
	class Thread_state;
	class Rm_client;
	class Platform_thread;
	class Platform_pd;

	/**
	 * Userland interface for the management of kernel thread-objects
	 */
	class Platform_thread : public Kernel_object<Kernel::Thread>
	{
		enum { LABEL_MAX_LEN = 32 };

		Platform_pd *                 _pd;
		Weak_ptr<Address_space>       _address_space;
		Pager_object *                _pager;
		Native_utcb *                 _utcb_core_addr; /* UTCB addr in core */
		Native_utcb *                 _utcb_pd_addr;   /* UTCB addr in pd   */
		Ram_dataspace_capability      _utcb;           /* UTCB dataspace    */
		char                          _label[LABEL_MAX_LEN];

		/*
		 * Wether this thread is the main thread of a program.
		 * This should be used only after 'join_pd' was called
		 * or if this is a core thread. For core threads its save
		 * also without 'join_pd' because '_main_thread' is initialized
		 * with 0 wich is always true as cores main thread has no
		 * 'Platform_thread'.
		 */
		bool _main_thread;

		Affinity::Location _location;

		/**
		 * Common construction part
		 */
		void _init();

		/*
		 * Check if this thread will attach its UTCB by itself
		 */
		bool _attaches_utcb_by_itself();

		unsigned _priority(unsigned virt_prio)
		{
			return Cpu_session::scale_priority(Kernel::Cpu_priority::MAX,
			                                   virt_prio);
		}

		public:

			/**
			 * Constructor for core threads
			 *
			 * \param label       debugging label
			 * \param utcb        virtual address of UTCB within core
			 */
			Platform_thread(const char * const label, Native_utcb * utcb);

			/**
			 * Constructor for threads outside of core
			 *
			 * \param quota      CPU quota that shall be granted to the thread
			 * \param label      debugging label
			 * \param virt_prio  unscaled processor-scheduling priority
			 * \param utcb       core local pointer to userland stack
			 */
			Platform_thread(size_t const quota, const char * const label,
			                unsigned const virt_prio, Affinity::Location,
			                addr_t const utcb);

			/**
			 * Destructor
			 */
			~Platform_thread();

			/**
			 * Join a protection domain
			 *
			 * \param pd             platform pd object pointer
			 * \param main_thread    wether thread is the first in protection domain
			 * \param address_space  corresponding Genode address space
			 *
			 * This function has no effect when called more twice for a
			 * given thread.
			 */
			void join_pd(Platform_pd *  const pd, bool const main_thread,
			             Weak_ptr<Address_space> address_space);

			/**
			 * Run this thread
			 *
			 * \param ip  initial instruction pointer
			 * \param sp  initial stack pointer
			 */
			int start(void * const ip, void * const sp);

			/**
			 * Pause this thread
			 */
			void pause() { Kernel::pause_thread(kernel_object()); }

			/**
			 * Enable/disable single stepping
			 */
			void single_step(bool) { }

			/**
			 * Resume this thread
			 */
			void resume() { Kernel::resume_thread(kernel_object()); }

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking() {
				Kernel::cancel_thread_blocking(kernel_object()); }

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const quota);

			/**
			 * Get raw thread state
			 */
			Thread_state state();

			/**
			 * Override raw thread state
			 */
			void state(Thread_state s);

			/**
			 * Return unique identification of this thread as faulter
			 */
			unsigned long pager_object_badge() { return (unsigned long)this; }

			/**
			 * Set the executing CPU for this thread
			 *
			 * \param location  targeted location in affinity space
			 */
			void affinity(Affinity::Location const & location);

			/**
			 * Get the executing CPU for this thread
			 */
			Affinity::Location affinity() const;

			/**
			 * Return the address space to which the thread is bound
			 */
			Weak_ptr<Address_space>& address_space();

			/**
			 * Return execution time consumed by the thread
			 */
			unsigned long long execution_time() const { return 0; }


			/***************
			 ** Accessors **
			 ***************/

			char const * label() const { return _label; };

			void pager(Pager_object * const pager);

			Pager_object * pager();

			Platform_pd * pd() const { return _pd; }

			Ram_dataspace_capability utcb() const { return _utcb; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
