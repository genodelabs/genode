/*
 * \brief  Core-specific instance of the VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-26
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <util/flex_iterator.h>
#include <dataspace/client.h>

/* core includes */
#include <vm_session_component.h>
#include <pd_session_component.h>
#include <cpu_thread_component.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Genode;


struct Vcpu_creation_error : Exception { };


Vcpu::Vcpu(Rpc_entrypoint            &ep,
           Constrained_ram_allocator &ram_alloc,
           Cap_quota_guard           &cap_alloc,
           Platform_thread           &thread,
           Cap_mapping               &task_cap,
           Vcpu_id_allocator         &vcpu_alloc)
:
	_ep(ep),
	_ram_alloc(ram_alloc),
	_cap_alloc(cap_alloc),
	_vcpu_ids(vcpu_alloc)
{
	Foc::l4_msgtag_t msg = l4_factory_create_irq(Foc::L4_BASE_FACTORY_CAP,
	                                             _recall.local.data()->kcap());
	if (l4_error(msg)) {
		Genode::error("vcpu irq creation failed", l4_error(msg));
		throw Vcpu_creation_error();
	}

	try {
		unsigned const vcpu_id = (unsigned)_vcpu_ids.alloc();
		_task_index_client = thread.setup_vcpu(vcpu_id, task_cap, recall_cap(),
	                                           _foc_vcpu_state);
		if (_task_index_client == Foc::L4_INVALID_CAP) {
			vcpu_alloc.free(vcpu_id);
			if (l4_error(Foc::l4_irq_detach(_recall.local.data()->kcap())))
				error("cannot detach IRQ");
			throw Vcpu_creation_error();
		}
	} catch (Vcpu_id_allocator::Out_of_indices) {
		throw Vcpu_creation_error();
	}

	_ep.manage(this);
}


Vcpu::~Vcpu()
{
	_ep.dissolve(this);

	if (_task_index_client != Foc::L4_INVALID_CAP) {
		if (l4_error(Foc::l4_irq_detach(_recall.local.data()->kcap())))
			error("cannot detach IRQ");
	}

	if (_foc_vcpu_state) {
		addr_t const vcpu_id = ((addr_t)_foc_vcpu_state -
		                          Platform::VCPU_VIRT_EXT_START) / L4_PAGESIZE;
		_vcpu_ids.free(vcpu_id);
	}
}


/**************************
 ** Vm_session_component **
 **************************/

Vm_session_component::Vm_session_component(Rpc_entrypoint &ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram,
                                           Region_map &local_rm,
                                           unsigned,
                                           Trace::Source_registry &)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_constrained_md_ram_alloc(ram, _ram_quota_guard(), _cap_quota_guard()),
	_heap(_constrained_md_ram_alloc, local_rm)
{
	Cap_quota_guard::Reservation caps(_cap_quota_guard(), Cap_quota{1});

	using namespace Foc;
	l4_msgtag_t msg = l4_factory_create_vm(L4_BASE_FACTORY_CAP,
	                                       _task_vcpu.local.data()->kcap());
	if (l4_error(msg)) {
		Genode::error("create_vm failed ", l4_error(msg));
		throw Service_denied();
	}

	/* configure managed VM area */
	_map.add_range(0, 0UL - 0x1000);
	_map.add_range(0UL - 0x1000, 0x1000);

	caps.acknowledge();
}


Vm_session_component::~Vm_session_component()
{
	_vcpus.for_each([&] (Vcpu &vcpu) {
		destroy(_heap, &vcpu); });

	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		if (!_map.any_block_addr(&out_addr))
			break;

		detach(out_addr);
	}
}


Capability<Vm_session::Native_vcpu> Vm_session_component::create_vcpu(Thread_capability cap)
{
	if (!cap.valid())
		return { };

	/* allocate vCPU object */
	Vcpu * vcpu = nullptr;

	_ep.apply(cap, [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		try {
			vcpu = new (_heap) Registered<Vcpu>(_vcpus,
			                                    _ep,
			                                    _constrained_md_ram_alloc,
			                                    _cap_quota_guard(),
			                                    thread->platform_thread(),
			                                    _task_vcpu,
			                                    _vcpu_ids);
		} catch (Vcpu_creation_error) {
			return;
		}
	});

	return vcpu ? vcpu->cap() : Capability<Vm_session::Native_vcpu> {};
}


void Vm_session_component::_attach_vm_memory(Dataspace_component &dsc,
                                             addr_t const guest_phys,
                                             Attach_attr const attribute)
{
	Flexpage_iterator flex(dsc.phys_addr() + attribute.offset, attribute.size,
	                       guest_phys, attribute.size, guest_phys);

	using namespace Foc;

	uint8_t flags = L4_FPAGE_RO;
	if (dsc.writeable() && attribute.writeable)
		if (attribute.executable)
			flags = L4_FPAGE_RWX;
		else
			flags = L4_FPAGE_RW;
	else
		if (attribute.executable)
			flags = L4_FPAGE_RX;

	Flexpage page = flex.page();
	while (page.valid()) {
		l4_fpage_t fp = l4_fpage(page.addr, (unsigned)page.log2_order, flags);
		l4_msgtag_t msg = l4_task_map(_task_vcpu.local.data()->kcap(),
		                              L4_BASE_TASK_CAP, fp,
		                              l4_map_obj_control(page.hotspot,
		                                                 L4_MAP_ITEM_MAP));

		if (l4_error(msg))
			Genode::error("task map failed ", l4_error(msg));

		page = flex.page();
	}
}


void Vm_session_component::_detach_vm_memory(addr_t guest_phys, size_t size)
{
	Flexpage_iterator flex(guest_phys, size, guest_phys, size, 0);
	Flexpage page = flex.page();

	while (page.valid()) {
		using namespace Foc;

		l4_task_unmap(_task_vcpu.local.data()->kcap(),
		              l4_fpage(page.addr, (unsigned)page.log2_order, L4_FPAGE_RWX),
		              L4_FP_ALL_SPACES);

		page = flex.page();
	}
}
