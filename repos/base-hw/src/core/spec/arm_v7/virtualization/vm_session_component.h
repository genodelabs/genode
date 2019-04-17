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

#ifndef _CORE__SPEC__ARM_V7__VIRTUALIZATION__VM_SESSION_COMPONENT_H_
#define _CORE__SPEC__ARM_V7__VIRTUALIZATION__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/allocator_avl.h>
#include <base/session_object.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>
#include <hw/spec/arm/lpae.h>

/* Core includes */
#include <object.h>
#include <region_map_component.h>
#include <translation_table.h>
#include <kernel/vm.h>

#include <trace/source_registry.h>

namespace Genode {
	class Vm_session_component;
}

class Genode::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public Rpc_object<Vm_session, Vm_session_component>,
	public Region_map_detach,
	private Kernel_object<Kernel::Vm>
{
	private:

		typedef Allocator_avl_tpl<Rm_region> Avl_region;

		/*
		 * Noncopyable
		 */
		Vm_session_component(Vm_session_component const &);
		Vm_session_component &operator = (Vm_session_component const &);

		using Table = Hw::Level_1_stage_2_translation_table;
		using Array = Table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

		Rpc_entrypoint            &_ep;
		Constrained_ram_allocator  _constrained_md_ram_alloc;
		Sliced_heap                _sliced_heap;
		Avl_region                 _map { &_sliced_heap };
		Region_map                &_region_map;
		Ram_dataspace_capability   _ds_cap  { };
		Region_map::Local_addr     _ds_addr { 0 };
		Table                     &_table;
		Array                     &_table_array;

		static size_t _ds_size() {
			return align_addr(sizeof(Cpu_state_modes),
			                  get_page_size_log2()); }

		addr_t _alloc_ds();
		void * _alloc_table();
		void   _attach(addr_t phys_addr, addr_t vm_addr, size_t size);

		void _attach_vm_memory(Dataspace_component &, addr_t, Attach_attr);
		void _detach_vm_memory(addr_t, size_t);

	protected:

		Ram_quota_guard &_ram_quota_guard() { return *this; }
		Cap_quota_guard &_cap_quota_guard() { return *this; }

	public:

		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;
		using Rpc_object<Vm_session, Vm_session_component>::cap;

		Vm_session_component(Rpc_entrypoint &, Resources, Label const &,
		                     Diag, Ram_allocator &ram, Region_map &, unsigned,
		                     Trace::Source_registry &);
		~Vm_session_component();

		/*********************************
		 ** Region_map_detach interface **
		 *********************************/

		void detach(Region_map::Local_addr) override;
		void unmap_region(addr_t, size_t) override;

		/**************************
		 ** Vm session interface **
		 **************************/

		Dataspace_capability _cpu_state(Vcpu_id) { return _ds_cap; }
		void _exception_handler(Signal_context_capability, Vcpu_id);
		void _run(Vcpu_id);
		void _pause(Vcpu_id);
		void attach(Dataspace_capability, addr_t, Attach_attr) override;
		void attach_pic(addr_t) override;
		void detach(addr_t, size_t) override;
		void _create_vcpu(Thread_capability) {}
};

#endif /* _CORE__SPEC__ARM_V7__VIRTUALIZATION__VM_SESSION_COMPONENT_H_ */
