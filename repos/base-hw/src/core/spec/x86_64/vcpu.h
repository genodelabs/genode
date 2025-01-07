/*
 * \brief  Vm_session vCPU
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2024-11-26
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
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
#include <kernel/vm.h>

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

		Kernel::Vm::Identity      &_id;
		Rpc_entrypoint            &_ep;
		Vcpu_data                  _vcpu_data { };
		Kernel_object<Kernel::Vm>  _kobj { };
		Constrained_ram_allocator &_ram;
		Ram_dataspace_capability   _ds_cap { };
		Region_map                &_region_map;
		Affinity::Location         _location;
		Phys_allocated<Data_pages> _vcpu_data_pages;

		constexpr size_t vcpu_state_size()
		{
			return align_addr(sizeof(Board::Vcpu_state),
			                  get_page_size_log2());
		}

	public:

		Vcpu(Kernel::Vm::Identity      &id,
		     Rpc_entrypoint            &ep,
		     Constrained_ram_allocator &constrained_ram_alloc,
		     Region_map                &region_map,
		     Affinity::Location         location)
		:
			_id(id),
			_ep(ep),
			_ram(constrained_ram_alloc),
			_ds_cap( {_ram.alloc(vcpu_state_size(), Cache::UNCACHED)} ),
			_region_map(region_map),
			_location(location),
			_vcpu_data_pages(ep, constrained_ram_alloc, region_map)
		{
			Region_map::Attr attr { };
			attr.writeable = true;
			_vcpu_data.vcpu_state = _region_map.attach(_ds_cap, attr).convert<Vcpu_state *>(
				[&] (Region_map::Range range) { return (Vcpu_state *)range.start; },
				[&] (Region_map::Attach_error) -> Vcpu_state * {
					error("failed to attach VCPU data within core");
					return nullptr;
				});

			if (!_vcpu_data.vcpu_state) {
				_ram.free(_ds_cap);

				throw Attached_dataspace::Region_conflict();
			}

			_vcpu_data.virt_area = &_vcpu_data_pages.obj;
			_vcpu_data.phys_addr = _vcpu_data_pages.phys_addr();

			ep.manage(this);
		}

		~Vcpu()
		{
			_region_map.detach((addr_t)_vcpu_data.vcpu_state);
			_ram.free(_ds_cap);
			_ep.dissolve(this);
		}

		/*******************************
		 ** Native_vcpu RPC interface **
		 *******************************/

		Capability<Dataspace>   state() const { return _ds_cap; }
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
				warning("Cannot instantiate vm kernel object, invalid signal context?");
		}
};

#endif /* _CORE__VCPU_H_ */
