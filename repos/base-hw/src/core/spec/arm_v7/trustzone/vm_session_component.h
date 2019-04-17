/*
 * \brief  Core-specific instance of the VM session interface
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V7__TRUSTZONE__VM_SESSION_COMPONENT_H_
#define _CORE__SPEC__ARM_V7__TRUSTZONE__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/session_object.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>
#include <trace/source_registry.h>

/* Core includes */
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

		/*
		 * Noncopyable
		 */
		Vm_session_component(Vm_session_component const &);
		Vm_session_component &operator = (Vm_session_component const &);

		Rpc_entrypoint            *_ds_ep;
		Constrained_ram_allocator  _constrained_md_ram_alloc;
		Region_map                &_region_map;
		Ram_dataspace_capability   _ds_cap  { };
		Region_map::Local_addr     _ds_addr { 0 };

		static size_t _ds_size() {
			return align_addr(sizeof(Cpu_state_modes),
			                  get_page_size_log2()); }

		addr_t _alloc_ds();

	protected:

		Ram_quota_guard &_ram_quota_guard() { return *this; }
		Cap_quota_guard &_cap_quota_guard() { return *this; }

	public:

		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;
		using Rpc_object<Vm_session, Vm_session_component>::cap;

		Vm_session_component(Rpc_entrypoint &, Resources, Label const &,
		                     Diag, Ram_allocator &ram, Region_map &,
		                     unsigned priority, Trace::Source_registry &);
		~Vm_session_component();

		/**************************
		 ** Vm session interface **
		 **************************/

		Dataspace_capability _cpu_state(Vcpu_id) { return _ds_cap; }
		void _exception_handler(Signal_context_capability handler, Vcpu_id);
		void _run(Vcpu_id);
		void _pause(Vcpu_id);

		void attach(Dataspace_capability, addr_t, Attach_attr) override {
			warning("Not implemented for TrustZone case"); }

		void attach_pic(addr_t /* vm_addr */) override {
			warning("Not implemented for TrustZone case"); }

		void detach(addr_t /* vm_addr */, size_t /* size */) override {
			warning("Not implemented for TrustZone case"); }
		void _create_vcpu(Thread_capability) {}
};

#endif /* _CORE__SPEC__ARM_V7__TRUSTZONE__VM_SESSION_COMPONENT_H_ */
