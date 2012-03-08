/*
 * \brief   L4/Fiasco protection domain facility
 * \author  Christian Helmuth
 * \author  Stefan Kalkowski
 * \date    2006-04-11
 *
 * Protection domains are L4 tasks under Fiasco.OC and serve as base
 * container for the platform.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/sync_allocator.h>
#include <platform_thread.h>
#include <base/thread.h>

namespace Fiasco {
#include <l4/sys/consts.h>
}

namespace Genode {

	class Platform_thread;
	class Platform_pd
	{
		private:

			enum {
				THREAD_MAX      = (1 << 6),
				UTCB_AREA_SIZE  = (THREAD_MAX * Fiasco::L4_UTCB_OFFSET),
				UTCB_AREA_START = (Thread_base::CONTEXT_AREA_VIRTUAL_BASE +
				                  THREAD_MAX * Thread_base::CONTEXT_VIRTUAL_SIZE)
			};

			Native_task       _l4_task_cap; /* L4 task capability slot */
			unsigned          _badge;
			Native_capability _parent;
			bool              _parent_cap_mapped;
			bool              _task_cap_mapped;
			Platform_thread  *_threads[THREAD_MAX];

			/**
			 * Protection-domain creation
			 *
			 * The syscall parameter propagates if any L4 kernel function
			 * should be used. We need the special case for the Core startup.
			 */
			void _create_pd(bool syscall);

			/**
			 * Protection domain destruction
			 *
			 * No special case for Core here - we just never call it.
			 */
			void _destroy_pd();

		public:

			class Threads_exhausted : Exception {};


			/**
			 * Constructor
			 */
			Platform_pd(bool create = true,
			            Native_task task_cap = Native_task());

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Bind thread to protection domain
			 *
			 * \return  0  on success or
			 *         -1  if thread ID allocation failed.
			 *
			 * This function allocates the physical L4 thread ID.
			 */
			int bind_thread(Platform_thread *thread);

			/**
			 * Unbind thread from protection domain
			 *
			 * Free the thread's slot and update thread object.
			 */
			void unbind_thread(Platform_thread *thread);

			/**
			 * Assign parent interface to protection domain
			 */
			int assign_parent(Native_capability parent);

			void map_task_cap();
			void map_parent_cap();

			/*******************************
			 ** Fiasco-specific Accessors **
			 *******************************/

			Native_task   native_task() { return _l4_task_cap;  }
			unsigned      badge()       { return _badge;        }
			Native_thread parent_cap()  { return _parent.tid(); }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
