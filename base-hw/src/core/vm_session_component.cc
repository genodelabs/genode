/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>
#include <util/arg_string.h>
#include <root/root.h>
#include <cpu/cpu_state.h>

/* core includes */
#include <vm_session_component.h>

using namespace Genode;


void Vm_session_component::exception_handler(Signal_context_capability handler)
{
	if (_vm_id) {
		PWRN("Cannot register exception_handler repeatedly");
		return;
	}

	_vm_id = Kernel::new_vm(_vm, (void*)_ds.core_local_addr(), handler.dst());
}


void Vm_session_component::run(void)
{
	if (!_vm_id) {
		PWRN("No exception handler registered!");
		return;
	}
	Kernel::run_vm(_vm_id);
}


Vm_session_component::Vm_session_component(Rpc_entrypoint  *ds_ep,
                                           Range_allocator *ram_alloc,
                                           size_t           ram_quota)
: _ds_ep(ds_ep), _ram_alloc(ram_alloc), _vm_id(0),
  _ds_addr(_alloc_ds(&ram_quota)),
  _ds(_ds_size(), _ds_addr, _ds_addr, false, true, 0),
  _ds_cap(static_cap_cast<Dataspace>(_ds_ep->manage(&_ds)))
{
	/* alloc needed memory */
	if (Kernel::vm_size() > ram_quota ||
		!_ram_alloc->alloc(Kernel::vm_size(), &_vm))
		throw Root::Quota_exceeded();
}


Vm_session_component::~Vm_session_component()
{
	/* dissolve VM dataspace from service entry point */
	_ds_ep->dissolve(&_ds);

	/* free region in allocator */
	_ram_alloc->free((void*)_ds.core_local_addr());
	_ram_alloc->free(_vm);
}
