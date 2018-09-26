/*
 * \brief  Core-specific instance of the VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
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

namespace Fiasco {
#include <l4/sys/factory.h>
}


using namespace Genode;


Vm_session_component::Vm_session_component(Rpc_entrypoint &ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram,
                                           Region_map &local_rm)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_constrained_md_ram_alloc(ram, _ram_quota_guard(), _cap_quota_guard()),
	_sliced_heap(_constrained_md_ram_alloc, local_rm)
{
	_cap_quota_guard().withdraw(Cap_quota{1});

	using namespace Fiasco;
	l4_msgtag_t msg = l4_factory_create_vm(L4_BASE_FACTORY_CAP,
	                                       _task_vcpu.local.data()->kcap());
	if (l4_error(msg)) {
		_cap_quota_guard().replenish(Cap_quota{1});
		Genode::error("create_vm failed ", l4_error(msg));
		throw 1;
	}
}

Vm_session_component::~Vm_session_component()
{
	_cap_quota_guard().replenish(Cap_quota{1});

	for (;Vcpu * vcpu = _vcpus.first();) {
		_vcpus.remove(vcpu);
		destroy(_sliced_heap, vcpu);
	}
}


Vcpu::Vcpu(Constrained_ram_allocator &ram_alloc,
           Cap_quota_guard &cap_alloc,
           Vm_session::Vcpu_id const id)
:
	_ram_alloc(ram_alloc),
	_cap_alloc(cap_alloc),
	_id(id)
{
	try {
		/* create ds for vCPU state */
		_ds_cap = _ram_alloc.alloc(4096, Cache_attribute::CACHED);
	} catch (...) {
		throw;
	}

	Fiasco::l4_msgtag_t msg = l4_factory_create_irq(Fiasco::L4_BASE_FACTORY_CAP,
	                                                _recall.local.data()->kcap());
	if (l4_error(msg)) {
		_ram_alloc.free(_ds_cap);
		Genode::error("vcpu irq creation failed", l4_error(msg));
		throw 1;
	}
}

Vcpu::~Vcpu()
{
	if (_ds_cap.valid())
		_ram_alloc.free(_ds_cap);
}

void Vm_session_component::_create_vcpu(Thread_capability cap)
{
	if (!cap.valid())
		return;

	auto lambda = [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		/* allocate vCPU object */
		Vcpu * vcpu = nullptr;
		try {
			vcpu = new (_sliced_heap) Vcpu(_constrained_md_ram_alloc,
	                                       _cap_quota_guard(),
	                                       Vcpu_id {_id_alloc});

			Fiasco::l4_cap_idx_t task = thread->platform_thread().setup_vcpu(_id_alloc, _task_vcpu, vcpu->recall_cap());
			if (task == Fiasco::L4_INVALID_CAP)
				throw 0;

			_ep.apply(vcpu->ds_cap(), [&] (Dataspace_component *ds) {
				if (!ds)
					throw 1;
				/* tell client where to find task cap */
				*reinterpret_cast<Fiasco::l4_cap_idx_t *>(ds->phys_addr()) = task;
			});
		} catch (int) {
			if (vcpu)
				destroy(_sliced_heap, vcpu);

			return;
		} catch (...) {
			if (vcpu)
				destroy(_sliced_heap, vcpu);

			throw;
		}

		_vcpus.insert(vcpu);
		_id_alloc++;
	};

	_ep.apply(cap, lambda);
}

Dataspace_capability Vm_session_component::_cpu_state(Vcpu_id const vcpu_id)
{
	for (Vcpu *vcpu = _vcpus.first(); vcpu; vcpu = vcpu->next()) {
		if (!vcpu->match(vcpu_id))
			continue;

		return vcpu->ds_cap();
	}

	return Dataspace_capability();
}

void Vm_session_component::attach(Dataspace_capability cap, addr_t guest_phys)
{
	if (!cap.valid())
		throw Invalid_dataspace();

	/* check dataspace validity */
	_ep.apply(cap, [&] (Dataspace_component *ptr) {
		if (!ptr)
			throw Invalid_dataspace();

		Dataspace_component &dsc = *ptr;

		/* unsupported - deny otherwise arbitrary physical memory can be mapped to a VM */
		if (dsc.managed())
			throw Invalid_dataspace();

		Flexpage_iterator flex(dsc.phys_addr(), dsc.size(),
		                       guest_phys, dsc.size(), guest_phys);

		using namespace Fiasco;

		uint8_t const flags = dsc.writable() ? L4_FPAGE_RWX : L4_FPAGE_RX;

		Flexpage page = flex.page();
		while (page.valid()) {
			l4_fpage_t fp = l4_fpage(page.addr, page.log2_order, flags);
			l4_msgtag_t msg = l4_task_map(_task_vcpu.local.data()->kcap(),
			                              L4_BASE_TASK_CAP, fp,
			                              l4_map_obj_control(page.hotspot,
			                                                 L4_MAP_ITEM_MAP));

			if (l4_error(msg))
				Genode::error("task map failed ", l4_error(msg));

			page = flex.page();
		}
	});
}
