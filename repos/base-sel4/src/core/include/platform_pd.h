/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
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


namespace Genode { class Platform_pd; }


class Genode::Platform_pd : public Address_space
{
	public:

		/**
		 * Constructors
		 */
		Platform_pd(Allocator * md_alloc, size_t ram_quota,
		            char const *, signed pd_id = -1, bool create = true);

		/**
		 * Destructor
		 */
		~Platform_pd();

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

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
