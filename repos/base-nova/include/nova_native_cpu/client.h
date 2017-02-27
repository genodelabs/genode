/*
 * \brief  Client-side NOVA-specific CPU session interface
 * \author Norman Feske
 * \date   2016-04-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA_NATIVE_CPU__CLIENT_H_
#define _INCLUDE__NOVA_NATIVE_CPU__CLIENT_H_

#include <nova_native_cpu/nova_native_cpu.h>
#include <base/rpc_client.h>

namespace Genode { struct Nova_native_cpu_client; }


struct Genode::Nova_native_cpu_client : Rpc_client<Nova_native_cpu>
{
	explicit Nova_native_cpu_client(Capability<Native_cpu> cap)
	: Rpc_client<Nova_native_cpu>(static_cap_cast<Nova_native_cpu>(cap)) { }

	void thread_type(Thread_capability thread_cap, Thread_type thread_type,
	                 Exception_base exception_base) {
		call<Rpc_thread_type>(thread_cap, thread_type, exception_base); }
};

#endif /* _INCLUDE__NOVA_NATIVE_CPU__CLIENT_H_ */
