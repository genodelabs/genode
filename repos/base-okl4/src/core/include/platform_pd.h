/*
 * \brief   OKL4-specific protection-domain facility
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <address_space.h>

namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

namespace Genode {

	namespace Thread_id_bits {

		/*
		 * L4 thread ID has 18 bits for thread number and 14 bits for
		 * version info.
		 */
		enum { PD = 8, THREAD = 5 };
	}

	class Platform_thread;
	class Platform_pd : public Address_space
	{
		private:

			friend class Platform_thread;

			enum { PD_INVALID  = -1,
			       PD_FIRST    = 0,
			       PD_MAX      = (1 << Thread_id_bits::PD) - 1,
			       THREAD_MAX  = (1 << Thread_id_bits::THREAD) - 1 };

			unsigned         _pd_id;        /* plain pd number */
			Platform_thread *_space_pager;  /* pager of the new pd */

			/**
			 * Manually construct L4 thread ID from its components
			 */
			static Okl4::L4_ThreadId_t make_l4_id(unsigned space_no,
			                                      unsigned thread_no)
			{
				/*
				 * On OKL4, version must be set to 1
				 */
				return Okl4::L4_GlobalId((space_no << Thread_id_bits::THREAD) | thread_no, 1);
			}


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

				Pd_alloc(bool r, bool f)
				: reserved(r), free(f) { }

				Pd_alloc() : reserved(0), free(0) { }
			};

			static Pd_alloc *_pds()
			{
				static Pd_alloc static_pds[PD_MAX + 1];
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
			 */
			int _alloc_pd();

			/**
			 * Protection domain deallocation
			 *
			 * No special case for Core here - we just never call it.
			 */
			void _free_pd();

			/**
			 * Setup UTCB area
			 */
			void _setup_address_space();


			/***************
			 ** Debugging **
			 ***************/

			void _debug_log_pds(void);
			void _debug_log_threads(void);

		public:

			/**
			 * Constructors
			 */
			Platform_pd(bool core);
			Platform_pd(Allocator *, char const *);

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Bind thread to protection domain
			 *
			 * This function allocates the physical L4 thread ID.
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

			Platform_thread* space_pager() const { return _space_pager; }

			void space_pager(Platform_thread *pd);

			int pd_id() const { return _pd_id; }


			/*****************************
			 ** Address-space interface **
			 *****************************/

			void flush(addr_t, size_t);
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
