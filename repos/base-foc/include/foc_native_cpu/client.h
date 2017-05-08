/*
 * \brief  Client-side Fiasco.OC specific CPU session interface
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC_NATIVE_CPU__CLIENT_H_
#define _INCLUDE__FOC_NATIVE_CPU__CLIENT_H_

#include <foc_native_cpu/foc_native_cpu.h>
#include <base/rpc_client.h>

namespace Genode { struct Foc_native_cpu_client; }


struct Genode::Foc_native_cpu_client : Rpc_client<Foc_native_cpu>
{
	explicit Foc_native_cpu_client(Capability<Native_cpu> cap)
	: Rpc_client<Foc_native_cpu>(static_cap_cast<Foc_native_cpu>(cap)) { }

	Native_capability native_cap(Thread_capability cap) override {
		return call<Rpc_native_cap>(cap); }

	Foc_thread_state thread_state(Thread_capability cap) override {
		return call<Rpc_thread_state>(cap); }
};

#endif /* _INCLUDE__FOC_NATIVE_CPU__CLIENT_H_ */
