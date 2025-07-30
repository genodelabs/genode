/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <vm_root.h>
#include <vm_session_component.h>

using namespace Genode;
using namespace Core;


/***********************
 ** Board::Vcpu_state **
 ***********************/

Board::Vcpu_state::Vcpu_state(Rpc_entrypoint          &ep,
                              Accounted_ram_allocator &ram,
                              Local_rm                &local_rm,
                              Ram_allocator::Result   &ds)
:
	_local_rm(local_rm),
	_hw_context(ep, ram, local_rm)
{
	ds.with_result([&] (Ram::Allocation &allocation) {
		using State = Genode::Vcpu_state;
		Region_map::Attr attr { };
		attr.writeable = true;
		_state = _local_rm.attach(allocation.cap,
		                          attr).convert<State *>(
			[&] (Local_rm::Attachment &a) {
				a.deallocate = false; return (State *)a.ptr; },
			[&] (Local_rm::Error) -> State * {
				error("failed to attach VCPU data within core");
				return nullptr;
			});

		if (!_state)
			throw Attached_dataspace::Region_conflict();
	},
	[&] (Ram::Error e) { throw_exception(e); });
}


Board::Vcpu_state::~Vcpu_state()
{
	_local_rm.detach((addr_t)_state);
}


/*******************
 ** Core::Vm_root **
 *******************/

Core::Vm_root::Create_result Vm_root::_create_session(const char *args)
{
	Session::Resources resources = session_resources_from_args(args);

	if (Hw::Virtualization_support::has_svm())
		return *new (md_alloc())
			Svm_session_component(_registry,
			                      _vmid_alloc,
			                      *ep(),
			                      resources,
			                      session_label_from_args(args),
			                      session_diag_from_args(args),
			                      _ram_allocator, _local_rm,
			                      _trace_sources);

	if (Hw::Virtualization_support::has_vmx())
		return *new (md_alloc())
			Vmx_session_component(_registry,
			                      _vmid_alloc,
			                      *ep(),
			                      session_resources_from_args(args),
			                      session_label_from_args(args),
			                      session_diag_from_args(args),
			                      _ram_allocator, _local_rm,
			                      _trace_sources);

	Genode::error( "No virtualization support detected.");
	throw Core::Service_denied();
}
