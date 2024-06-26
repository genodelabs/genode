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

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <address_space.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

namespace Core {

	class Platform_thread;
	class Platform_pd;
}


class Core::Platform_pd : public Address_space
{
	public:

		struct Thread_id { unsigned value; };

		enum class Alloc_thread_id_error { EXHAUSTED };
		using Alloc_thread_id_result = Attempt<Thread_id, Alloc_thread_id_error>;

	private:

		/*
		 * Noncopyable
		 */
		Platform_pd(Platform_pd const &);
		Platform_pd &operator = (Platform_pd const &);

		enum {
			VERSION_BITS   = 10,
			VERSION_MASK   = (1 << VERSION_BITS) - 1,
			PD_FIRST       = 0x10,
			PD_MAX         = (1 << 11) - 1, /* leave 0x7ff free for L4_INVALID_ID */
			PD_VERSION_MAX = (1 << 10) - 1,
			PD_INVALID     = -1,
			THREAD_MAX     = (1 << 7),
		};

		unsigned _pd_id   = 0;
		unsigned _version = 0;

		Fiasco::l4_taskid_t _l4_task_id { };  /* L4 task ID */


		/***************************************
		 ** Threads of this protection domain **
		 ***************************************/

		Platform_thread *_threads[THREAD_MAX] { };


		/******************
		 ** PD allocator **
		 ******************/

		struct Pd_alloc
		{
			unsigned reserved : 1;
			unsigned free     : 1;
			unsigned version  : VERSION_BITS;

			Pd_alloc(bool r, bool f, unsigned v)
			: reserved(r), free(f), version(v & VERSION_MASK) { }

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
		Platform_pd(Allocator &md_alloc, char const *name);

		/**
		 * Constructor used for core's PD
		 */
		Platform_pd(char const *name, signed pd_id);

		/**
		 * Destructor
		 */
		~Platform_pd();

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t) { }

		/**
		 * Initialize L4 task facility
		 */
		static void init();

		/**
		 * Allocate PD-local ID for a new 'Platform_thread'
		 */
		Alloc_thread_id_result alloc_thread_id(Platform_thread &);

		/**
		 * Release PD-local thread ID at destruction of 'Platform_thread'
		 */
		void free_thread_id(Thread_id);

		/**
		 * Return L4 thread ID from the PD's task ID and the PD-local thread ID
		 */
		Fiasco::l4_threadid_t l4_thread_id(Thread_id const id) const
		{
			Fiasco::l4_threadid_t result = _l4_task_id;
			enum { LTHREAD_MASK = (1 << 7) - 1 };
			result.id.lthread = id.value & LTHREAD_MASK;
			return result;
		}

		/**
		 * Assign parent interface to protection domain
		 */
		void assign_parent(Native_capability) { }

		int pd_id() const { return _pd_id; }


		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(addr_t, size_t, Core_local_addr) override;
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
