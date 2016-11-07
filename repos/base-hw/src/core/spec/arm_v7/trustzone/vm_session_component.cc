/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <kernel/core_interface.h>
#include <vm_session_component.h>
#include <core_env.h>

using namespace Genode;


void Vm_session_component::exception_handler(Signal_context_capability handler)
{
	if (!create((void*)_ds.core_local_addr(), Capability_space::capid(handler),
				nullptr))
	{
		warning("Cannot instantiate vm kernel object twice,"
		        "or invalid signal context?");
	}
}


Vm_session_component::Vm_session_component(Rpc_entrypoint  *ds_ep,
                                           size_t           ram_quota)
: _ds_ep(ds_ep), _ds(_ds_size(), _alloc_ds(ram_quota), UNCACHED, true, 0),
  _ds_cap(static_cap_cast<Dataspace>(_ds_ep->manage(&_ds)))
{
	_ds.assign_core_local_addr(core_env()->rm_session()->attach(_ds_cap));
}


Vm_session_component::~Vm_session_component()
{
	/* dissolve VM dataspace from service entry point */
	_ds_ep->dissolve(&_ds);

	/* free region in allocator */
	core_env()->rm_session()->detach(_ds.core_local_addr());
	platform()->ram_alloc()->free((void*)_ds.phys_addr());
}
