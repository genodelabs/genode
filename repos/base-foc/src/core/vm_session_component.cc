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

using namespace Core;


struct Vcpu_creation_error : Exception { };


Vcpu::Vcpu(Rpc_entrypoint          &ep,
           Accounted_ram_allocator &ram_alloc,
           Cap_quota_guard         &cap_alloc,
           Platform_thread         &thread,
           Cap_mapping             &task_cap,
           Vcpu_id_allocator       &vcpu_alloc)
:
	_ep(ep),
	_ram_alloc(ram_alloc),
	_cap_alloc(cap_alloc),
	_vcpu_ids(vcpu_alloc)
{
	Foc::l4_msgtag_t msg = l4_factory_create_irq(Foc::L4_BASE_FACTORY_CAP,
	                                             _recall.local.data()->kcap());
	if (l4_error(msg)) {
		error("vcpu irq creation failed", l4_error(msg));
		return;
	}

	constructed =
		_vcpu_ids.alloc().convert<Constructed>(
			[&] (auto const vcpu_id) -> Constructed {
				_task_index_client = thread.setup_vcpu(vcpu_id, task_cap,
				                                       recall_cap(),
				                                       _foc_vcpu_state);

				if (_task_index_client == Foc::L4_INVALID_CAP) {
					(void)vcpu_alloc.free(vcpu_id);
					if (l4_error(Foc::l4_irq_detach(_recall.local.data()->kcap())))
						error("cannot detach IRQ");
					return Error();
				}
				return Ok();
		},
		[&] (Vcpu_id_allocator::Error) { return Error(); });

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
                                           Ram_allocator &ram,
                                           Local_rm &local_rm,
                                           unsigned,
                                           Trace::Source_registry &)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_ram(ram, _ram_quota_guard(), _cap_quota_guard()),
	_memory(ep, *this, _ram, local_rm),
	_heap(_ram, local_rm)
{
	Cap_quota_guard::Result caps = _cap_quota_guard().reserve(Cap_quota{1});
	if (caps.failed())
		constructed = Session_error::OUT_OF_CAPS;

	using namespace Foc;
	l4_msgtag_t msg = l4_factory_create_vm(L4_BASE_FACTORY_CAP,
	                                       _task_vcpu.local.data()->kcap());
	if (l4_error(msg)) {
		error("create_vm failed ", l4_error(msg));
		constructed = Session_error::DENIED;
	}

	caps.with_result([&] (Cap_quota_guard::Reservation &r) { r.deallocate = false; },
	                 [&] (Cap_quota_guard::Error) { /* handled at 'reserve' */ });

	constructed = Ok();
}


Vm_session_component::~Vm_session_component()
{
	_vcpus.for_each([&] (Vcpu &vcpu) {
		destroy(_heap, &vcpu); });
}


Vm_session_component::Create_vcpu_result
Vm_session_component::create_vcpu(Thread_capability cap)
{
	Create_vcpu_result result = Create_vcpu_error::DENIED;

	if (!cap.valid())
		return result;

	_ep.apply(cap, [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		result =
			_vcpu_alloc.create(_vcpus, _ep, _ram, _cap_quota_guard(),
			                   thread->platform_thread(), _task_vcpu,
			                   _vcpu_ids).template convert<Create_vcpu_result>(
			[&] (auto &a) {
				return a.obj.constructed.template convert<Create_vcpu_result>(
					[&] (auto) {
						a.deallocate = false;
						return a.obj.cap(); },
					[&] (auto) { return Create_vcpu_error::DENIED; });
			},
			[] (auto err) { return err; });
	});

	return result;
}


Vm_session_component::Attach_result
Vm_session_component::attach(Dataspace_capability const cap,
                             addr_t const guest_phys, Attach_attr attribute)
{
	auto map_fn = [&] (addr_t vm_addr, addr_t phys_addr, size_t size,
	                   bool exec, bool write, Cache) -> Attach_result
	{
		Flexpage_iterator flex(phys_addr, size, vm_addr, size, vm_addr);

		using namespace Foc;

		uint8_t flags = write ? (exec ? L4_FPAGE_RWX : L4_FPAGE_RW)
		                      : (exec ? L4_FPAGE_RX  : L4_FPAGE_RO);

		Flexpage page = flex.page();
		while (page.valid()) {
			l4_fpage_t fp = l4_fpage(page.addr, (unsigned)page.log2_order, flags);
			l4_msgtag_t msg = l4_task_map(_task_vcpu.local.data()->kcap(),
			                              L4_BASE_TASK_CAP, fp,
			                              l4_map_obj_control(page.hotspot,
			                                                 L4_MAP_ITEM_MAP));

			if (l4_error(msg)) {
				error("task map failed ", l4_error(msg));
				return Attach_error::INVALID_DATASPACE;
			}
			page = flex.page();
		}

		return Region_map::Range(vm_addr, size);
	};

	return _memory.attach(cap, guest_phys, attribute,
	                      [&] (addr_t vm_addr, addr_t phys_addr, size_t size,
	                           bool exec, bool write, Cache cacheable) {
		return map_fn(vm_addr, phys_addr, size, exec,
		                         write, cacheable); });
}


void Vm_session_component::_detach(addr_t guest_phys, size_t size)
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


void Vm_session_component::detach(addr_t guest_phys, size_t size)
{
	_memory.detach(guest_phys, size, [&](addr_t vm_addr, size_t size) {
		_detach(vm_addr, size); });
}


void Vm_session_component::detach_at(addr_t const addr)
{
	_memory.detach_at(addr, [&](addr_t vm_addr, size_t size) {
		_detach(vm_addr, size); });
}


void Vm_session_component::reserve_and_flush(addr_t const addr)
{
	_memory.reserve_and_flush(addr, [&](addr_t vm_addr, size_t size) {
		_detach(vm_addr, size); });
}
