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

#include <base/stdint.h>
#include <nova/receive_window.h>

namespace Genode { struct Native_thread; }

struct Genode::Native_thread
{
	static constexpr unsigned long INVALID_INDEX = ~0UL;

	addr_t ec_sel     { 0 };      /* selector for execution context */
	addr_t exc_pt_sel { 0 };      /* base of event portal window */
	bool   vcpu       { false };  /* true if thread is a virtual CPU */
	addr_t initial_ip { 0 };      /* initial IP of local thread */

	/* receive window for capability selectors received at the server side */
	Receive_window server_rcv_window { };

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

	void reset_client_rcv_sel() { client_rcv_sel = INVALID_INDEX; }

	Native_capability pager_cap { };

	Native_thread() : ec_sel(INVALID_INDEX),
	                  exc_pt_sel(INVALID_INDEX),
	                  vcpu(false),
	                  initial_ip(0) { }
};

#endif /* _INCLUDE__NOVA__NATIVE_THREAD_H_ */
