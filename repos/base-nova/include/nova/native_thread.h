/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 *
 * On most platforms, the 'Genode::Native_thread' type is private to the
 * base framework. However, on NOVA, we make the type publicly available to
 * expose the low-level thread-specific capability selectors to user-level
 * virtual-machine monitors (Seoul or VirtualBox).
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOVA__NATIVE_THREAD_H_
#define _INCLUDE__NOVA__NATIVE_THREAD_H_

#include <base/stdint.h>
#include <nova/receive_window.h>

namespace Genode { struct Native_thread; }

struct Genode::Native_thread
{
	enum { INVALID_INDEX = ~0UL };

	addr_t ec_sel;     /* selector for execution context */
	addr_t exc_pt_sel; /* base of event portal window */
	bool   vcpu;       /* true if thread is a virtual CPU */
	addr_t initial_ip; /* initial IP of local thread */

	/* receive window for capability selectors received at the server side */
	Receive_window rcv_window;

	Native_capability pager_cap;

	Native_thread() : ec_sel(INVALID_INDEX),
	                  exc_pt_sel(INVALID_INDEX),
	                  vcpu(false),
	                  initial_ip(0) { }
};

#endif /* _INCLUDE__NOVA__NATIVE_THREAD_H_ */
