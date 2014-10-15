/*
 * \brief  Platform-specific type definitions
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/stdint.h>
#include <base/native_capability.h>

namespace Genode {

	struct Cap_dst_policy
	{
		typedef int Dst;
		static bool valid(Dst tid) { return false; }
		static Dst  invalid()      { return true; }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	class Platform_thread;

	typedef int Native_thread_id;

	struct Native_thread
	{
		int id;

		/**
		 * Only used in core
		 *
		 * For 'Thread' objects created within core, 'pt' points to
		 * the physical thread object, which is going to be destroyed
		 * on destruction of the 'Thread'.
		 */
		Platform_thread *pt;
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	typedef struct { } Native_utcb;

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 */
		static constexpr addr_t context_area_virtual_base() {
			return 0x40000000UL; }
		static constexpr addr_t context_area_virtual_size() {
			return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static constexpr addr_t context_virtual_size() { return 0x00100000UL; }
	};

	struct Native_pd_args { };

	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
