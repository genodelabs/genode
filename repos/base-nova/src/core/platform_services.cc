/*
 * \brief  Platform specific services for NOVA
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
#include <core_env.h>
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
                                       Ram_allocator &)
{
	static Vm_root vm_root(ep, heap, core_env().ram_allocator(),
	                       core_env().local_rm(), trace_sources);
	static Core_service<Vm_session_component> vm(services, vm_root);

	static Io_port_root io_root(*core_env().pd_session(),
	                            platform().io_port_alloc(), heap);

	static Core_service<Io_port_session_component> io_port(services, io_root);
}
