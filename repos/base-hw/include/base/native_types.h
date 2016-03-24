/*
 * \brief  Basic Genode types
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/log.h>
#include <base/native_capability.h>
#include <base/ipc_msgbuf.h>
#include <base/printf.h>

namespace Genode
{
	class Platform_thread;
	class Native_thread;

	using Native_thread_id = Kernel::capid_t;

	typedef int Native_connection_state;

	/**
	 * Information that a thread creator hands out to a new thread
	 */
	class Start_info;

	/**
	 * Coherent address region
	 */
	struct Native_region;
}


struct Genode::Native_thread
{
	Platform_thread  * platform_thread;
	Native_capability  cap;
};


/**
 * Coherent address region
 */
struct Genode::Native_region
{
	addr_t base;
	size_t size;
};


namespace Genode
{
	static constexpr addr_t VIRT_ADDR_SPACE_START = 0x1000;
	static constexpr size_t VIRT_ADDR_SPACE_SIZE  = 0xfffee000;

	/**
	 * The main thread's UTCB, used during bootstrap of the main thread before it
	 * allocates its stack area, needs to be outside the virtual memory area
	 * controlled by the RM session, because it is needed before the main
	 * thread can access its RM session.
	 */
	static constexpr Native_utcb * utcb_main_thread() {
		return (Native_utcb *) (VIRT_ADDR_SPACE_START + VIRT_ADDR_SPACE_SIZE); }
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
