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

#include <base/native_capability.h>
#include <base/stdint.h>

namespace Genode {

	struct Cap_dst_policy {
		typedef int Dst;
		static bool valid(Dst) { return false; }
		static Dst invalid()   { return false; }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef volatile int  Native_lock;
	typedef int           Native_thread;
	typedef Native_thread Native_thread_id;
	typedef struct { }    Native_utcb;
	typedef int Native_connection_state;
	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 */
		static addr_t context_area_virtual_base() { return 0x40000000UL; }
		static addr_t context_area_virtual_size() { return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static addr_t context_virtual_size() { return 0x00100000UL; }
	};
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
