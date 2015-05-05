/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <platform_thread.h>
#include <address_space.h>

/* Codezero includes */
#include <codezero/syscalls.h>

namespace Genode {

	class Platform_thread;
	class Platform_pd : public Address_space
	{
		private:

			enum { MAX_THREADS_PER_PD = 32 };
			enum { UTCB_VIRT_BASE = 0x30000000 };
			enum { UTCB_AREA_SIZE = MAX_THREADS_PER_PD*sizeof(struct Codezero::utcb) };

			unsigned _space_id;

			bool utcb_in_use[MAX_THREADS_PER_PD];

		public:

			/**
			 * Constructors
			 */
			Platform_pd(bool core);
			Platform_pd(Allocator * md_alloc, char const *,
			            signed pd_id = -1, bool create = true);

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { }

			/**
			 * Bind thread to protection domain
			 *
			 * \return  0  on success or
			 *         -1  if thread ID allocation failed.
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
			int assign_parent(Native_capability parent) { return 0; }


			/*****************************
			 ** Address-space interface **
			 *****************************/

			void flush(addr_t, size_t) { PDBG("not implemented"); }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
