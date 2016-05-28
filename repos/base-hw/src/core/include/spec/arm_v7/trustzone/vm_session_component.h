/*
 * \brief  Core-specific instance of the VM session interface
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__ARM_V7__TRUSTZONE__VM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__SPEC__ARM_V7__TRUSTZONE__VM_SESSION_COMPONENT_H_

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
: public Genode::Rpc_object<Genode::Vm_session>,
  public Kernel_object<Kernel::Vm>
{
	private:

		Rpc_entrypoint      *_ds_ep;
		Range_allocator     *_ram_alloc;
		Dataspace_component  _ds;
		Dataspace_capability _ds_cap;
		addr_t               _ds_addr;

		static size_t _ds_size() {
			return align_addr(sizeof(Cpu_state_modes),
			                  get_page_size_log2()); }

		addr_t _alloc_ds(size_t &ram_quota);

	public:

		Vm_session_component(Rpc_entrypoint *ds_ep,
		                     size_t          ram_quota);
		~Vm_session_component();


		/**************************
		 ** Vm session interface **
		 **************************/

		Dataspace_capability cpu_state(void) { return _ds_cap; }
		void exception_handler(Signal_context_capability handler);
		void run(void);
		void pause(void);

		void attach(Dataspace_capability ds_cap, addr_t vm_addr) {
			PWRN("Not implemented for TrustZone case"); }

		void attach_pic(addr_t vm_addr) {
			PWRN("Not implemented for TrustZone case"); }

		void detach(addr_t vm_addr, size_t size) {
			PWRN("Not implemented for TrustZone case"); }
};

#endif /* _CORE__INCLUDE__SPEC__ARM_V7__TRUSTZONE__VM_SESSION_COMPONENT_H_ */
