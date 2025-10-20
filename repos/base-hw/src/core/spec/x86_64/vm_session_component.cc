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
#include <spec/x86_64/ept.h>
#include <spec/x86_64/hpt.h>
#include <vm_root.h>
#include <vm_session_component.h>

using namespace Genode;
using namespace Core;

template <>
void Vm_session_component<Hw::Ept>::attach_pic(addr_t) {}

template <>
void Vm_session_component<Hw::Hpt>::attach_pic(addr_t) {}


/*******************
 ** Core::Vm_root **
 *******************/

Core::Vm_root::Create_result Vm_root::_create_session(const char *args)
{
	using Vmx_session_component = Vm_session_component<Hw::Ept>;
	using Svm_session_component = Vm_session_component<Hw::Hpt>;

	Session::Resources resources = session_resources_from_args(args);

	if (Hw::Virtualization_support::has_svm())
		return *new (md_alloc())
			Svm_session_component(_registry,
			                      _vmid_alloc,
			                      *ep(),
			                      resources,
			                      session_label_from_args(args),
			                      session_diag_from_args(args),
			                      _ram_allocator, _mapped_ram, _local_rm,
			                      _trace_sources);

	if (Hw::Virtualization_support::has_vmx())
		return *new (md_alloc())
			Vmx_session_component(_registry,
			                      _vmid_alloc,
			                      *ep(),
			                      session_resources_from_args(args),
			                      session_label_from_args(args),
			                      session_diag_from_args(args),
			                      _ram_allocator, _mapped_ram, _local_rm,
			                      _trace_sources);

	Genode::error( "No virtualization support detected.");
	throw Core::Service_denied();
}
