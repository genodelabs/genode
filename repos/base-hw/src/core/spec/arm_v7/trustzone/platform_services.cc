/*
 * \brief   Platform specific services for base-hw (TrustZone)
 * \author  Stefan Kalkowski
 * \date    2012-10-26
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>

/* core includes */
#include <platform.h>
#include <platform_services.h>
#include <core_env.h>
#include <core_service.h>
#include <vm_root.h>
#include <map_local.h>


extern int monitor_mode_exception_vector;


/*
 * Add TrustZone specific vm service
 */
void Core::platform_add_local_services(Rpc_entrypoint    &ep,
                                       Sliced_heap       &sliced_heap,
                                       Registry<Service> &local_services,
                                       Core::Trace::Source_registry &trace_sources,
                                       Ram_allocator &)
{
	static addr_t const phys_base =
		Platform::core_phys_addr((addr_t)&monitor_mode_exception_vector);

	map_local(phys_base, Hw::Mm::system_exception_vector().base, 1,
	          Hw::PAGE_FLAGS_KERN_TEXT);

	static Vm_root vm_root(ep, sliced_heap, core_env().ram_allocator(),
	                       core_env().local_rm(), trace_sources);

	static Core_service<Vm_session_component> vm_service(local_services, vm_root);
}
