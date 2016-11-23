/*
 * \brief  Fiasco.OC-specific 'Native_cpu' setup
 * \author Christian Prochaska
 * \date   2016-05-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
#include <foc_native_cpu/client.h>

/* GDB monitor includes */
#include "cpu_session_component.h"
#include "cpu_thread_component.h"


namespace Gdb_monitor {
	class Native_cpu_component;
}


using namespace Genode;


class Gdb_monitor::Native_cpu_component : public Rpc_object<Foc_native_cpu,
                                                            Native_cpu_component>
{
	private:

		Cpu_session_component &_cpu_session_component;
		Foc_native_cpu_client  _foc_native_cpu;

	public:

		Native_cpu_component(Cpu_session_component &cpu_session_component)
		: _cpu_session_component(cpu_session_component),
		  _foc_native_cpu(_cpu_session_component.parent_cpu_session().native_cpu())
		{
			_cpu_session_component.thread_ep().manage(this);
		}

		~Native_cpu_component()
		{
			_cpu_session_component.thread_ep().dissolve(this);
		}

		void enable_vcpu(Thread_capability thread_cap, addr_t vcpu_state) override
		{
			Cpu_thread_component *cpu_thread = _cpu_session_component.lookup_cpu_thread(thread_cap);
			_foc_native_cpu.enable_vcpu(cpu_thread->parent_thread_cap(), vcpu_state);
		}

		Native_capability native_cap(Thread_capability thread_cap) override
		{
			Cpu_thread_component *cpu_thread = _cpu_session_component.lookup_cpu_thread(thread_cap);
			return _foc_native_cpu.native_cap(cpu_thread->parent_thread_cap());
		}

		Native_capability alloc_irq() override
		{
			return _foc_native_cpu.alloc_irq();
		}
};


Capability<Cpu_session::Native_cpu>
Gdb_monitor::Cpu_session_component::_setup_native_cpu()
{
	Native_cpu_component *native_cpu_component =
		new (_md_alloc) Native_cpu_component(*this);

	return native_cpu_component->cap();
}


void Gdb_monitor::Cpu_session_component::_cleanup_native_cpu()
{
    Native_cpu_component *native_cpu_component = nullptr;
    _ep.apply(_native_cpu_cap, [&] (Native_cpu_component *c) { native_cpu_component = c; });

    if (!native_cpu_component) return;

	destroy(_md_alloc, native_cpu_component);
}
