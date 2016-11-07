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
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/synced_allocator.h>
#include <platform_thread.h>
#include <base/thread.h>

/* core includes */
#include <cap_mapping.h>
#include <address_space.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/consts.h>
}

namespace Genode {

	class Platform_thread;
	class Platform_pd : public Address_space
	{
		private:

			enum {
				THREAD_MAX      = (1 << 7),
				UTCB_AREA_SIZE  = (THREAD_MAX * Fiasco::L4_UTCB_OFFSET),
			};

			addr_t utcb_area_start()
			{
				return stack_area_virtual_base() +
				       THREAD_MAX*stack_virtual_size();
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
			Platform_pd(Allocator *, char const *label);

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Bind thread to protection domain
			 */
			bool bind_thread(Platform_thread *thread);

			/**
			 * Unbind thread from protection domain
			 *
			 * Free the thread's slot and update thread object.
			 */
			void unbind_thread(Platform_thread *thread);

			/**
			 * Assign parent interface to protection domain
			 */
			void assign_parent(Native_capability parent);


			/*******************************
			 ** Fiasco-specific Accessors **
			 *******************************/

			Native_capability native_task() { return _task.local; }


			/*****************************
			 ** Address-space interface **
			 *****************************/

			/*
			 * On Fiasco.OC, we don't use directed unmap but rely on the
			 * in-kernel mapping database. See 'region_map_support.cc'.
			 */
			void flush(addr_t, size_t) { warning(__func__, " not implemented"); }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
