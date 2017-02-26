/*
 * \brief  Fiasco.OC-specific 'Native_cpu' setup
 * \author Christian Prochaska
 * \date   2016-05-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <foc_native_cpu/client.h>

/* GDB monitor includes */
#include "cpu_session_component.h"
#include "cpu_thread_component.h"


namespace Cpu_sampler {
	class Native_cpu_component;
}


using namespace Genode;


class Cpu_sampler::Native_cpu_component : public Rpc_object<Foc_native_cpu,
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
			auto lambda = [&] (Cpu_sampler::Cpu_thread_component *cpu_thread) {
				_foc_native_cpu.enable_vcpu(cpu_thread->parent_thread(), vcpu_state);
			};

			_cpu_session_component.thread_ep().apply(thread_cap, lambda);
		}

		Native_capability native_cap(Thread_capability thread_cap) override
		{
			auto lambda = [&] (Cpu_sampler::Cpu_thread_component *cpu_thread) {
				return _foc_native_cpu.native_cap(cpu_thread->parent_thread());
			};

			return _cpu_session_component.thread_ep().apply(thread_cap, lambda);
		}

		Native_capability alloc_irq() override
		{
			return _foc_native_cpu.alloc_irq();
		}

		Foc_thread_state thread_state(Thread_capability cap) override
		{
			auto lambda = [&] (Cpu_sampler::Cpu_thread_component *cpu_thread) {
				return _foc_native_cpu.thread_state(cpu_thread->parent_thread());
			};

			return _cpu_session_component.thread_ep().apply(cap, lambda);
		}
};


Capability<Cpu_session::Native_cpu>
Cpu_sampler::Cpu_session_component::_setup_native_cpu()
{
	Native_cpu_component *native_cpu_component =
		new (_md_alloc) Native_cpu_component(*this);

	return native_cpu_component->cap();
}


void Cpu_sampler::Cpu_session_component::_cleanup_native_cpu()
{
    Native_cpu_component *native_cpu_component = nullptr;
    _thread_ep.apply(_native_cpu_cap, [&] (Native_cpu_component *c) { native_cpu_component = c; });

    if (!native_cpu_component) return;

	destroy(_md_alloc, native_cpu_component);
}
