/*
 * \brief  Monitored kernel-specific native CPU interface
 * \author Norman Feske
 * \date   2023-05-16
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITORED_NATIVE_CPU_H_
#define _MONITORED_NATIVE_CPU_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <cpu_thread/client.h>

/* local includes */
#include <types.h>
#include <monitored_thread.h>
#include <native_cpu_nova.h>

namespace Monitor { struct Monitored_native_cpu_nova; }


struct Monitor::Monitored_native_cpu_nova : Monitored_rpc_object<Native_cpu_nova>
{
	using Monitored_rpc_object::Monitored_rpc_object;

	void thread_type(Capability<Cpu_thread> cap, Thread_type type,
	                 Exception_base exc) override
	{
		Monitored_thread::with_thread(_ep, cap,
			[&] (Monitored_thread &monitored_thread) {
				_real.call<Rpc_thread_type>(monitored_thread._real, type, exc); },
			[&] /* thread not intercepted */ {
				_real.call<Rpc_thread_type>(cap, type, exc); });
	}
};

#endif /* _MONITORED_NATIVE_CPU_H_ */
