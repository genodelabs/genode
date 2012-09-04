/*
 * \brief   Userland interface for the management of kernel thread-objects
 * \author  Martin Stein
 * \date    2012-02-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <ram_session/ram_session.h>
#include <base/native_types.h>
#include <kernel/syscalls.h>
#include <kernel/log.h>

/* core includes */
#include <assert.h>

namespace Genode {

	class Pager_object;
	class Thread_state;
	class Thread_base;
	class Rm_client;
	class Platform_thread;

	size_t kernel_thread_size();

	/**
	 * Userland interface for the management of kernel thread-objects
	 */
	class Platform_thread
	{
		enum { NAME_MAX_LEN = 32 };

		Thread_base *            _thread_base;
		unsigned long            _stack_size;
		unsigned long            _pd_id;
		unsigned long            _id;
		Rm_client *              _rm_client;
		bool                     _main_thread;
		Native_utcb *            _phys_utcb;
		Native_utcb *            _virt_utcb;
		Software_tlb *           _software_tlb;
		Ram_dataspace_capability _utcb;
		char                     _name[NAME_MAX_LEN];

		/**
		 * Common construction part
		 */
		void _init();

		public:

			/**
			 * Constructor for core threads
			 */
			Platform_thread(const char *        name,
			                Thread_base * const thread_base,
			                unsigned long const stack_size,
			                unsigned long const pd_id);

			/**
			 * Constructor for threads outside of core
			 */
			Platform_thread(const char * name, unsigned int priority,
			                addr_t utcb);

			/**
			 * Join PD identified by 'pd_id'
			 *
			 * \param pd_id        ID of targeted PD
			 * \param main_thread  wether we are the main thread in this PD
			 *
			 * \retval  0  on success
			 * \retval <0  otherwise
			 */
			int join_pd(unsigned long const pd_id,
			            bool const main_thread);

			/**
			 * Run this thread
			 */
			int start(void * ip, void * sp, unsigned int cpu_no = 0);

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
			void cancel_blocking()
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			};

			/**
			 * Request our raw thread state
			 *
			 * \param state_dst  destination state buffer
			 *
			 * \retval  0  successful
			 * \retval -1  thread state not accessible
			 */
			int state(Genode::Thread_state * state_dst)
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
				return -1;
			};

			/**
			 * Destructor
			 */
			~Platform_thread()
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			}

			/**
			 * Return unique identification of this thread as faulter
			 */
			unsigned long pager_object_badge() { return _id; }

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(unsigned cpu)
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			};


			/***************
			 ** Accessors **
			 ***************/

			inline char const * name() const { return _name; }

			void pager(Pager_object * const pager);

			Pager_object * pager() const;

			unsigned long pd_id() const { return _pd_id; }

			Native_thread_id id() const { return _id; }

			unsigned long stack_size() const { return _stack_size; }

			Thread_base * thread_base()
			{
				if (!_thread_base) assert(_main_thread);
				return _thread_base;
			}

			Native_utcb * phys_utcb() const { return _phys_utcb; }

			Native_utcb * virt_utcb() const { return _virt_utcb; }

			Ram_dataspace_capability utcb() const { return _utcb; }

			Software_tlb * software_tlb() const { return _software_tlb; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */

