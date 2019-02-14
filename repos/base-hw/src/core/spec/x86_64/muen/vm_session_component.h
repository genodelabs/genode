/*
 * \brief  Core-specific instance of the VM session interface
 * \author Stefan Kalkowski
 * \date   2015-06-03
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__MUEN__VM_SESSION_COMPONENT_H_
#define _CORE__SPEC__X86_64__MUEN__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>

/* Core includes */
#include <dataspace_component.h>
#include <object.h>
#include <kernel/vm.h>

namespace Genode {
	class Vm_session_component;
}

class Genode::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public Rpc_object<Vm_session, Vm_session_component>,
	private Kernel_object<Kernel::Vm>
{
	private:

		Vm_state _state;

	public:

		Vm_session_component(Rpc_entrypoint &, Resources resources,
		                     Label const &, Diag, Ram_allocator &, Region_map &)
		:
			Ram_quota_guard(resources.ram_quota),
			Cap_quota_guard(resources.cap_quota),
			_state()
		{ }

		~Vm_session_component() { }

		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;
		using Genode::Rpc_object<Genode::Vm_session, Vm_session_component>::cap;

		/**************************
		 ** Vm session interface **
		 **************************/

		Dataspace_capability _cpu_state(Vcpu_id) { return Dataspace_capability(); }

		void _exception_handler(Signal_context_capability handler, Vcpu_id)
		{
			if (!create(&_state, Capability_space::capid(handler), nullptr))
				warning("Cannot instantiate vm kernel object, "
				        "invalid signal context?");
		}

		void _run(Vcpu_id)
		{
			if (Kernel_object<Kernel::Vm>::_cap.valid())
				Kernel::run_vm(kernel_object());
		}

		void _pause(Vcpu_id)
		{
			if (Kernel_object<Kernel::Vm>::_cap.valid())
				Kernel::pause_vm(kernel_object());
		}

		void attach(Dataspace_capability, addr_t) override { }
		void attach_pic(addr_t)                   override { }
		void detach(addr_t, size_t)               override { }
		void _create_vcpu(Thread_capability) { }
};

#endif /* _CORE__SPEC__X86_64__MUEN__VM_SESSION_COMPONENT_H_ */
