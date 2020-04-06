/*
 * \brief   Platform specific services for ARM with virtualization
 * \author  Stefan Kalkowski
 * \date    2014-07-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>
#include <base/heap.h>

/* core includes */
#include <platform.h>
#include <platform_services.h>
#include <core_env.h>
#include <core_service.h>
#include <map_local.h>
#include <vm_root.h>
#include <platform.h>

extern Genode::addr_t hypervisor_exception_vector;

/*
 * Add ARM virtualization specific vm service
 */
void Genode::platform_add_local_services(Rpc_entrypoint         &ep,
                                         Sliced_heap            &sh,
                                         Registry<Service>      &services,
                                         Trace::Source_registry &trace_sources)
{
	using namespace Genode;

	map_local(Platform::core_phys_addr((addr_t)&hypervisor_exception_vector),
	          Hw::Mm::hypervisor_exception_vector().base,
	          Hw::Mm::hypervisor_exception_vector().size / get_page_size(),
	          Hw::PAGE_FLAGS_KERN_TEXT);

	void * stack = nullptr;
	assert(platform().ram_alloc().alloc_aligned(Hw::Mm::hypervisor_stack().size,
	                                             (void**)&stack,
	                                             get_page_size_log2()).ok());
	map_local((addr_t)stack,
	          Hw::Mm::hypervisor_stack().base,
	          Hw::Mm::hypervisor_stack().size / get_page_size(),
	          Hw::PAGE_FLAGS_KERN_DATA);

	static Vm_root vm_root(ep, sh, core_env().ram_allocator(),
	                       core_env().local_rm(), trace_sources);
	static Core_service<Vm_session_component> vm_service(services, vm_root);
}
