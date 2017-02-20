/*
 * \brief  Kernel-specific part of the CPU-session interface
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__NATIVE_CPU_COMPONENT_H_
#define _CORE__INCLUDE__NATIVE_CPU_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <foc_native_cpu/foc_native_cpu.h>

namespace Genode {

	class Cpu_session_component;
	class Native_cpu_component;
}


class Genode::Native_cpu_component : public Rpc_object<Foc_native_cpu,
                                                       Native_cpu_component>
{
	private:

		Cpu_session_component &_cpu_session;
		Rpc_entrypoint        &_thread_ep;

	public:

		Native_cpu_component(Cpu_session_component &, char const *);
		~Native_cpu_component();

		void enable_vcpu(Thread_capability, addr_t) override;
		Native_capability native_cap(Thread_capability) override;
		Native_capability alloc_irq() override;
		Foc_thread_state thread_state(Thread_capability) override;
};

#endif /* _CORE__INCLUDE__NATIVE_CPU_COMPONENT_H_ */
