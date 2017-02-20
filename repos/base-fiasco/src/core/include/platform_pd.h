/*
 * \brief   L4/Fiasco protection domain facility
 * \author  Christian Helmuth
 * \date    2006-04-11
 *
 * Protection domains are L4 tasks under Fiasco and serve as base
 * container for the platform.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <base/allocator.h>
#include <platform_thread.h>
#include <address_space.h>

namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Platform_thread;
	class Platform_pd : public Address_space
	{
		private:

			enum {
				VERSION_BITS   = 10,
				PD_FIRST       = 0x10,
				PD_MAX         = (1 << 11) - 1, /* leave 0x7ff free for L4_INVALID_ID */
				PD_VERSION_MAX = (1 << 10) - 1,
				PD_INVALID     = -1,
				THREAD_MAX     = (1 << 7),
			};

			unsigned _pd_id;    /* plain pd number */
			unsigned _version;  /* version number */

			Fiasco::l4_taskid_t _l4_task_id;  /* L4 task ID */


			/**********************************************
			 ** Threads of this protection domain object **
			 **********************************************/

			Platform_thread *_threads[THREAD_MAX];

			/**
			 * Initialize thread allocator
			 */
			void _init_threads();

			/**
			 * Thread iteration for one task
			 */
			Platform_thread *_next_thread();

			/**
			 * Thread allocation
			 *
			 * Again a special case for Core thread0.
			 */
			int _alloc_thread(int thread_id, Platform_thread *thread);

			/**
			 * Thread deallocation
			 *
			 * No special case for Core thread0 here - we just never call it.
			 */
			void _free_thread(int thread_id);


			/******************
			 ** PD allocator **
			 ******************/

			struct Pd_alloc
			{
				unsigned reserved : 1;
				unsigned free     : 1;
				unsigned version  : VERSION_BITS;

				Pd_alloc(bool r, bool f, unsigned v)
				: reserved(r), free(f), version(v) { }

				Pd_alloc() : reserved(0), free(0), version(0) { }
			};

			static Pd_alloc *_pds()
			{
				static Pd_alloc static_pds[PD_MAX];
				return static_pds;
			}

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

			/**
			 * Protection domain allocation
			 *
			 * Find free L4 task and use it. We need the special case for Core
			 * startup.
			 */
			int _alloc_pd(signed pd_id);

			/**
			 * Protection domain deallocation
			 *
			 * No special case for Core here - we just never call it.
			 */
			void _free_pd();


			/***************
			 ** Debugging **
			 ***************/

			void _debug_log_pds(void);
			void _debug_log_threads(void);

		public:

			/**
			 * Constructor
			 */
			Platform_pd(Allocator * md_alloc, char const *,
			            signed pd_id = PD_INVALID, bool create = true);

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { }

			/**
			 * Initialize L4 task facility
			 */
			static void init();

			/**
			 * Bind thread to protection domain
			 *
			 * \return true on success
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
			void assign_parent(Native_capability parent) { }

			int pd_id() const { return _pd_id; }


			/*****************************
			 ** Address-space interface **
			 *****************************/

			/*
			 * On L4/Fiasco, we don't use directed unmap but rely on the
			 * in-kernel mapping database. See 'region_map_support.cc'.
			 */
			void flush(addr_t, size_t) { warning(__func__, " not implemented"); }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
