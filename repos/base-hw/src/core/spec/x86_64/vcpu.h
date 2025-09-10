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


class Core::Vcpu : public Rpc_object<Vm_session::Native_vcpu, Vcpu>,
                   public Revoke
{
	private:

		Kernel::Vcpu::Identity     &_id;
		Rpc_entrypoint             &_ep;
		Ram_allocator::Result       _ds;
		Signal_context_capability   _sigh_cap { };
		Affinity::Location          _location;
		Board::Vcpu_state           _state;
		Kernel_object<Kernel::Vcpu> _kobj { };

		constexpr size_t _ds_size()
		{
			return align_addr(sizeof(Genode::Vcpu_state),
			                  get_page_size_log2());
		}

	public:

		using Constructed = Attempt<Ok, Accounted_mapped_ram_allocator::Error>;
		Constructed const constructed = _state.constructed;

		Vcpu(Kernel::Vcpu::Identity         &id,
		     Rpc_entrypoint                 &ep,
		     Accounted_ram_allocator        &ram,
		     Local_rm                       &local_rm,
		     Accounted_mapped_ram_allocator &core_ram,
		     Affinity::Location              location)
		:
			_id(id),
			_ep(ep),
			_ds( {ram.try_alloc(_ds_size(), Cache::UNCACHED)} ),
			_location(location),
			_state(core_ram, local_rm, _ds)
		{
			ep.manage(this);
		}

		~Vcpu()
		{
			_ep.dissolve(this);
		}

		void revoke_signal_context(Signal_context_capability cap) override
		{
			if (cap != _sigh_cap)
				return;

			_sigh_cap = Signal_context_capability();
			if (_kobj.constructed()) _kobj.destruct();
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

			_sigh_cap = handler;

			unsigned const cpu = _location.xpos();

			if (!_kobj.create(cpu, (void *)&_state,
			                  Capability_space::capid(handler), _id))
				warning("Cannot instantiate vcpu kernel object, invalid signal context?");
		}
};

#endif /* _CORE__VCPU_H_ */
