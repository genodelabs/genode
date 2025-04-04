/*
 * \brief  Vm_session vCPU
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2024-11-26
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__VCPU_H_
#define _CORE__VCPU_H_

/* base includes */
#include <base/attached_dataspace.h>
#include <vm_session/vm_session.h>

/* base-hw includes */
#include <hw_native_vcpu/hw_native_vcpu.h>
#include <kernel/vcpu.h>

/* core includes */
#include <phys_allocated.h>
#include <region_map_component.h>

namespace Core { struct Vcpu; }


class Core::Vcpu : public Rpc_object<Vm_session::Native_vcpu, Vcpu>
{
	private:
		struct Data_pages {
			uint8_t _[Vcpu_data::size()];
		};

		Kernel::Vcpu::Identity     &_id;
		Rpc_entrypoint             &_ep;
		Vcpu_data                   _vcpu_data { };
		Kernel_object<Kernel::Vcpu> _kobj { };
		Accounted_ram_allocator    &_ram;
		Ram_allocator::Result       _ds;
		Local_rm                   &_local_rm;
		Affinity::Location          _location;
		Phys_allocated<Data_pages>  _vcpu_data_pages;

		constexpr size_t vcpu_state_size()
		{
			return align_addr(sizeof(Board::Vcpu_state),
			                  get_page_size_log2());
		}

	public:

		Vcpu(Kernel::Vcpu::Identity  &id,
		     Rpc_entrypoint          &ep,
		     Accounted_ram_allocator &ram,
		     Local_rm                &local_rm,
		     Affinity::Location       location)
		:
			_id(id),
			_ep(ep),
			_ram(ram),
			_ds( {_ram.try_alloc(vcpu_state_size(), Cache::UNCACHED)} ),
			_local_rm(local_rm),
			_location(location),
			_vcpu_data_pages(ep, ram, local_rm)
		{
			_ds.with_result([&] (Ram::Allocation &allocation) {
				Region_map::Attr attr { };
				attr.writeable = true;
				_vcpu_data.vcpu_state = _local_rm.attach(allocation.cap, attr).convert<Vcpu_state *>(
					[&] (Local_rm::Attachment &a) {
						a.deallocate = false; return (Vcpu_state *)a.ptr; },
					[&] (Local_rm::Error) -> Vcpu_state * {
						error("failed to attach VCPU data within core");
						return nullptr;
					});

				if (!_vcpu_data.vcpu_state)
					throw Attached_dataspace::Region_conflict();

				_vcpu_data.virt_area = &_vcpu_data_pages.obj;
				_vcpu_data.phys_addr = _vcpu_data_pages.phys_addr();

				ep.manage(this);
			},
			[&] (Ram::Error e) { throw_exception(e); });
		}

		~Vcpu()
		{
			_local_rm.detach((addr_t)_vcpu_data.vcpu_state);
			_ep.dissolve(this);
		}

		/*******************************
		 ** Native_vcpu RPC interface **
		 *******************************/

		Capability<Dataspace> state() const
		{
			return _ds.convert<Capability<Dataspace>>(
				[&] (Ram::Allocation const &ds) { return ds.cap; },
				[&] (Ram::Error) { return Capability<Dataspace>(); });
		}

		Native_capability native_vcpu()       { return _kobj.cap(); }

		void exception_handler(Signal_context_capability handler)
		{
			using Genode::warning;
			if (!handler.valid()) {
				warning("invalid signal");
				return;
			}

			if (_kobj.constructed()) {
				warning("Cannot register vcpu handler twice");
				return;
			}

			unsigned const cpu = _location.xpos();

			if (!_kobj.create(cpu, (void *)&_vcpu_data,
			                  Capability_space::capid(handler), _id))
				warning("Cannot instantiate vcpu kernel object, invalid signal context?");
		}
};

#endif /* _CORE__VCPU_H_ */
