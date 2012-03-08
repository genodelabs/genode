/*
 * \brief  Native types on Pistachio
 * \author Norman Feske
 * \date   2008-07-26
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>

namespace Pistachio {
#include <l4/types.h>

	struct Thread_id_checker
	{
		static bool valid(L4_ThreadId_t tid) { return !L4_IsNilThread(tid); }
		static L4_ThreadId_t invalid() { return L4_nilthread; }
	};
}

namespace Genode {

	typedef volatile int Native_lock;

	class Platform_thread;

	typedef Pistachio::L4_ThreadId_t Native_thread_id;

	struct Native_thread
	{
		Native_thread_id l4id;

		/**
		 * Only used in core
		 *
		 * For 'Thread' objects created within core, 'pt' points to
		 * the physical thread object, which is going to be destroyed
		 * on destruction of the 'Thread'.
		 */
		Platform_thread *pt;
	};

	inline unsigned long convert_native_thread_id_to_badge(Native_thread_id tid)
	{
		/*
		 * Pistachio has no server-defined badges for page-fault messages.
		 * Therefore, we have to interpret the sender ID as badge.
		 */
		return tid.raw;
	}

	/**
	 * Empty UTCB type expected by the thread library
	 *
	 * On this kernel, UTCBs are not placed within the the context area. Each
	 * thread can request its own UTCB pointer using the kernel interface.
	 */
	typedef struct { } Native_utcb;

	typedef Native_capability_tpl<Pistachio::L4_ThreadId_t,
	                              Pistachio::Thread_id_checker> Native_capability;
	typedef Pistachio::L4_ThreadId_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
