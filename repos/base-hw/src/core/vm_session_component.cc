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

/* Genode includes */
#include <util/string.h>
#include <util/arg_string.h>
#include <root/root.h>
#include <cpu/cpu_state.h>

/* core includes */
#include <kernel/core_interface.h>
#include <vm_session_component.h>
#include <platform.h>
#include <core_env.h>

using namespace Genode;


addr_t Vm_session_component::_alloc_ds(size_t &ram_quota)
{
	addr_t addr;
	if (_ds_size() > ram_quota ||
		platform()->ram_alloc()->alloc_aligned(_ds_size(), (void**)&addr,
		                                       get_page_size_log2()).is_error())
		throw Root::Quota_exceeded();
	ram_quota -= _ds_size();
	return addr;
}


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


void Vm_session_component::pause(void)
{
	if (!_vm_id) {
		PWRN("No exception handler registered!");
		return;
	}
	Kernel::pause_vm(_vm_id);
}


Vm_session_component::Vm_session_component(Rpc_entrypoint  *ds_ep,
                                           size_t           ram_quota)
: _ds_ep(ds_ep), _vm_id(0),
  _ds(_ds_size(), _alloc_ds(ram_quota), UNCACHED, true, 0),
  _ds_cap(static_cap_cast<Dataspace>(_ds_ep->manage(&_ds)))
{
	_ds.assign_core_local_addr(core_env()->rm_session()->attach(_ds_cap));

	/* alloc needed memory */
	if (Kernel::vm_size() > ram_quota ||
		!platform()->core_mem_alloc()->alloc(Kernel::vm_size(), &_vm))
		throw Root::Quota_exceeded();
}


Vm_session_component::~Vm_session_component()
{
	/* dissolve VM dataspace from service entry point */
	_ds_ep->dissolve(&_ds);

	/* free region in allocator */
	core_env()->rm_session()->detach(_ds.core_local_addr());
	platform()->ram_alloc()->free((void*)_ds.phys_addr());
	platform()->core_mem_alloc()->free(_vm);
}
