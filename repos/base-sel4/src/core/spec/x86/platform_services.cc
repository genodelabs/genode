/*
 * \brief  Platform specific services for seL4 x86
 * \author Alexander Boettcher
 * \date   2018-08-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_services.h>
#include <vm_root.h>
#include <io_port_root.h>

/*
 * Add x86 specific services 
 */
void Core::platform_add_local_services(Rpc_entrypoint         &ep,
                                       Sliced_heap            &heap,
                                       Registry<Service>      &services,
                                       Trace::Source_registry &trace_sources,
                                       Ram_allocator          &core_ram,
                                       Region_map             &core_rm,
                                       Range_allocator        &io_port_ranges)
{
	static Vm_root vm_root(ep, heap, core_ram, core_rm, trace_sources);

	static Core_service<Vm_session_component> vm(services, vm_root);

	static Io_port_root io_root(io_port_ranges, heap);

	static Core_service<Io_port_session_component> io_port(services, io_root);
}
