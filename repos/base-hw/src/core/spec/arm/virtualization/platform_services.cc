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

/* core includes */
#include <platform.h>
#include <platform_services.h>
#include <core_env.h>
#include <core_service.h>
#include <map_local.h>
#include <vm_root.h>
#include <platform.h>

using namespace Core;


extern addr_t hypervisor_exception_vector;


/*
 * Add ARM virtualization specific vm service
 */
void Core::platform_add_local_services(Rpc_entrypoint    &ep,
                                       Sliced_heap       &sh,
                                       Registry<Service> &services,
                                       Core::Trace::Source_registry &trace_sources,
                                       Ram_allocator &)
{
	map_local(Platform::core_phys_addr((addr_t)&hypervisor_exception_vector),
	          Hw::Mm::hypervisor_exception_vector().base,
	          Hw::Mm::hypervisor_exception_vector().size / get_page_size(),
	          Hw::PAGE_FLAGS_KERN_TEXT);

	platform().ram_alloc().alloc_aligned(Hw::Mm::hypervisor_stack().size,
	                                     get_page_size_log2()).with_result(
		[&] (void *stack) {
			map_local((addr_t)stack,
			          Hw::Mm::hypervisor_stack().base,
			          Hw::Mm::hypervisor_stack().size / get_page_size(),
			          Hw::PAGE_FLAGS_KERN_DATA);

			static Vm_root vm_root(ep, sh, core_env().ram_allocator(),
			                       core_env().local_rm(), trace_sources);
			static Core_service<Vm_session_component> vm_service(services, vm_root);
		},
		[&] (Range_allocator::Alloc_error) {
			warning("failed to allocate hypervisor stack for VM service");
		}
	);
}
