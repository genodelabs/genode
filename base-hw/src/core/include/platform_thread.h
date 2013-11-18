/*
 * \brief   Userland interface for the management of kernel thread-objects
 * \author  Martin Stein
 * \date    2012-02-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <ram_session/ram_session.h>
#include <base/native_types.h>
#include <base/thread.h>

/* base-hw includes */
#include <kernel/interface.h>
#include <kernel/log.h>

/* core includes */
#include <address_space.h>
#include <kernel/thread.h>

namespace Genode {

	class Pager_object;
	class Thread_state;
	class Rm_client;
	class Platform_thread;

	/**
	 * Userland interface for the management of kernel thread-objects
	 */
	class Platform_thread
	{
		enum { LABEL_MAX_LEN = 32 };

		size_t                   _stack_size;
		unsigned                 _pd_id;
		Weak_ptr<Address_space>  _address_space;
		unsigned                 _id;
		Rm_client *              _rm_client;
		Native_utcb *            _utcb_phys;
		Native_utcb *            _utcb_virt;
		Tlb *                    _tlb;
		Ram_dataspace_capability _utcb;
		char                     _label[LABEL_MAX_LEN];
		char                     _kernel_thread[sizeof(Kernel::Thread)];

		/*
		 * Wether this thread is the main thread of a program.
		 * This should be used only after 'join_pd' was called
		 * or if this is a core thread. For core threads its save
		 * also without 'join_pd' because '_main_thread' is initialized
		 * with 0 wich is always true as cores main thread has no
		 * 'Platform_thread'.
		 */
		bool _main_thread;

		/**
		 * Common construction part
		 */
		void _init();

		/*
		 * Check if this thread will attach its UTCB by itself
		 */
		bool _attaches_utcb_by_itself();

		public:

			/**
			 * Constructor for core threads
			 *
			 * \param label       debugging label
			 * \param stack_size  initial size of the stack
			 * \param pd_id       kernel name of targeted protection domain
			 */
			Platform_thread(const char * const label, size_t const stack_size,
			                unsigned const pd_id);

			/**
			 * Constructor for threads outside of core
			 *
			 * \param label     debugging label
			 * \param priority  processor-scheduling priority
			 * \param utcb      core local pointer to userland thread-context
			 */
			Platform_thread(const char * const label, unsigned const priority,
			                addr_t const utcb);

			/**
			 * Destructor
			 */
			~Platform_thread();

			/**
			 * Join a protection domain
			 *
			 * \param pd_id          kernel name of targeted protection domain
			 * \param main_thread    wether thread is the first in protection domain
			 * \param address_space  corresponding Genode address space
			 *
			 * \retval  0  succeeded
			 * \retval -1  failed
			 */
			int join_pd(unsigned const pd_id, bool const main_thread,
			            Weak_ptr<Address_space> address_space);

			/**
			 * Run this thread
			 *
			 * \param ip      initial instruction pointer
			 * \param sp      initial stack pointer
			 * \param cpu_id  kernel name of targeted CPU
			 */
			int start(void * const ip, void * const sp, unsigned const cpu_id = 0);

			/**
			 * Pause this thread
			 */
			void pause() { Kernel::pause_thread(_id); }

			/**
			 * Resume this thread
			 */
			void resume() { Kernel::resume_thread(_id); }

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking() { resume(); }

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
			unsigned pager_object_badge() { return (unsigned)this; }

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(Affinity::Location) { }

			/**
			 * Get the executing CPU for this thread
			 */
			Affinity::Location affinity() { return Affinity::Location(); };

			/**
			 * Return the address space to which the thread is bound
			 */
			Weak_ptr<Address_space> address_space();


			/***************
			 ** Accessors **
			 ***************/

			void pager(Pager_object * const pager);

			Pager_object * pager();

			unsigned pd_id() const { return _pd_id; }

			Native_thread_id id() const { return _id; }

			size_t stack_size() const { return _stack_size; }

			Native_utcb * utcb_phys() const { return _utcb_phys; }

			Ram_dataspace_capability utcb() const { return _utcb; }

			Tlb * tlb() const { return _tlb; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */

