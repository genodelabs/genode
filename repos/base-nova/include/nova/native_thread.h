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
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA__NATIVE_THREAD_H_
#define _INCLUDE__NOVA__NATIVE_THREAD_H_

#include <nova/receive_window.h>

namespace Genode { struct Native_thread; }


struct Genode::Native_thread : Noncopyable
{
	static constexpr unsigned long INVALID_INDEX = ~0UL;

	addr_t ec_sel     = INVALID_INDEX;   /* selector for execution context */
	addr_t exc_pt_sel = INVALID_INDEX;   /* base of event portal window */

	/*
	 * Designated selector to populate with the result of an IPC call
	 *
	 * By default, the client-side receive window for delegated selectors
	 * is automatically allocated within the component's selector space.
	 * However, in special cases such as during the initialization of a
	 * user-level VMM (ports/include/vmm/vcpu_dispatcher.h), the targeted
	 * selector is defined manually. The 'client_rcv_sel' provides the
	 * hook for such a manual allocation. If it contains a valid selector
	 * value, the value is used as the basis of the receive window of an
	 * 'ipc_call'.
	 */
	addr_t client_rcv_sel = INVALID_INDEX;

	addr_t initial_ip = 0;

	/* receive window for capability selectors received at the server side */
	Receive_window server_rcv_window { };

	Native_capability pager_cap { };

	Native_thread() { }

	/* ec_sel is invalid until thread gets started */
	bool ec_valid() const { return ec_sel != INVALID_INDEX; }

	void reset_client_rcv_sel() { client_rcv_sel = INVALID_INDEX; }
};

#endif /* _INCLUDE__NOVA__NATIVE_THREAD_H_ */
