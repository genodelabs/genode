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

#ifndef _CORE__INCLUDE__SPEC__ARM_V7__VIRTUALIZATION__VM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__SPEC__ARM_V7__VIRTUALIZATION__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>
#include <long_translation_table.h>

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

		using Translation_table =
			Genode::Level_1_stage_2_translation_table;
		using Table_allocator = Translation_table_allocator_tpl<
			Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

		Rpc_entrypoint              *_ds_ep;
		Range_allocator             *_ram_alloc;
		Dataspace_component          _ds;
		Dataspace_capability         _ds_cap;
		addr_t                       _ds_addr;
		Translation_table           *_table;
		Translation_table_allocator *_tt_alloc;

		static size_t _ds_size() {
			return align_addr(sizeof(Cpu_state_modes),
			                  get_page_size_log2()); }

		addr_t _alloc_ds(size_t &ram_quota);
		void   _attach(addr_t phys_addr, addr_t vm_addr, size_t size);

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
		void attach(Dataspace_capability ds_cap, addr_t vm_addr);
		void attach_pic(addr_t vm_addr);
		void detach(addr_t vm_addr, size_t size);
};

#endif /* _CORE__INCLUDE__SPEC__ARM_V7__VIRTUALIZATION__VM_SESSION_COMPONENT_H_ */
