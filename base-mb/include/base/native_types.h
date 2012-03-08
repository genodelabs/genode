/*
 * \brief  Dummy definitions for native types used for compiling unit tests
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <kernel/types.h>
#include <base/native_capability.h>

namespace Genode {

	typedef Kernel::Thread_id     Native_thread_id;
	typedef Native_thread_id      Native_thread;

	typedef Kernel::Protection_id Native_process_id;

	typedef Kernel::Utcb_unaligned                     Native_utcb;
	typedef Kernel::Paging::Physical_page::Permissions Native_page_permission;
	typedef Kernel::Paging::Physical_page::size_t      Native_page_size;

	Native_thread_id my_thread_id();


	struct Thread_id_check
	{
		static bool valid(Kernel::Thread_id tid) {
			return tid != Kernel::INVALID_THREAD_ID; }
		static Kernel::Thread_id invalid()
			{ return Kernel::INVALID_THREAD_ID; }
	};


	typedef Native_capability_tpl<Kernel::Thread_id,Thread_id_check> Native_capability;
	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */




