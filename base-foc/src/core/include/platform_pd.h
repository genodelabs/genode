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

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/sync_allocator.h>
#include <platform_thread.h>
#include <base/thread.h>

/* core includes */
#include <cap_mapping.h>

/* Fiasco.OC includes */
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
			};

			addr_t utcb_area_start()
			{
				return (Native_config::context_area_virtual_base() +
				       THREAD_MAX * Native_config::context_virtual_size());
			}

			Cap_mapping       _task;
			Cap_mapping       _parent;
			Platform_thread  *_threads[THREAD_MAX];

		public:

			class Threads_exhausted : Exception {};


			/**
			 * Constructor for core.
			 */
			Platform_pd(Core_cap_index*);

			/**
			 * Constructor for all tasks except core.
			 */
			Platform_pd();

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

			/*******************************
			 ** Fiasco-specific Accessors **
			 *******************************/

			Native_capability native_task() { return _task.local; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
