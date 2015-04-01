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
	if (_initialized) {
		PWRN("Cannot initialize kernel vm object twice!");
		return;
	}

	if (Kernel::new_vm(&_vm, (void*)_ds.core_local_addr(), handler.dst(), 0)) {
		PWRN("Cannot instantiate vm kernel object, invalid signal context?");
		return;
	}

	_initialized = true;
}


Vm_session_component::Vm_session_component(Rpc_entrypoint  *ds_ep,
                                           size_t           ram_quota)
: _ds_ep(ds_ep), _ds(_ds_size(), _alloc_ds(ram_quota), UNCACHED, true, 0),
  _ds_cap(static_cap_cast<Dataspace>(_ds_ep->manage(&_ds)))
{
	_ds.assign_core_local_addr(core_env()->rm_session()->attach(_ds_cap));
}
