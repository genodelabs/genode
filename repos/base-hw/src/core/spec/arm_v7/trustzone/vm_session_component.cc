/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/core_interface.h>
#include <vm_session_component.h>
#include <core_env.h>

using namespace Genode;


void Vm_session_component::_exception_handler(Signal_context_capability handler, Vcpu_id)
{
	if (!create(_ds_addr, Capability_space::capid(handler), nullptr))
	{
		warning("Cannot instantiate vm kernel object twice,"
		        "or invalid signal context?");
	}
}


Vm_session_component::Vm_session_component(Rpc_entrypoint  &ds_ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram_alloc,
                                           Region_map &region_map,
                                           unsigned)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ds_ep(&ds_ep),
	_constrained_md_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_region_map(region_map)
{
	_ds_cap = _constrained_md_ram_alloc.alloc(_ds_size(), Genode::Cache_attribute::UNCACHED);

	try {
		_ds_addr = region_map.attach(_ds_cap);
	} catch (...) {
		_constrained_md_ram_alloc.free(_ds_cap);
		throw;
	}
}


Vm_session_component::~Vm_session_component()
{
	/* free region in allocator */
	if (_ds_cap.valid()) {
		_region_map.detach(_ds_addr);
		_constrained_md_ram_alloc.free(_ds_cap);
	}
}
