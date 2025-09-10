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
#include <core_service.h>
#include <map_local.h>
#include <vm_root.h>

using namespace Core;


extern addr_t hypervisor_exception_vector;


/*
 * Add ARM virtualization specific vm service
 */
void Core::platform_add_local_services(Runtime                &,
                                       Rpc_entrypoint         &ep,
                                       Sliced_heap            &sh,
                                       Registry<Service>      &services,
                                       Trace::Source_registry &trace_sources,
                                       Ram_allocator          &core_ram,
                                       Mapped_ram_allocator   &mapped_ram,
                                       Local_rm               &local_rm,
                                       Range_allocator        &)
{
	map_local(Platform::core_phys_addr((addr_t)&hypervisor_exception_vector),
	          Hw::Mm::hypervisor_exception_vector().base,
	          Hw::Mm::hypervisor_exception_vector().size / get_page_size(),
	          Hw::PAGE_FLAGS_KERN_TEXT);

	platform().ram_alloc().alloc_aligned(Hw::Mm::hypervisor_stack().size,
	                                     get_page_size_log2()).with_result(
		[&] (Range_allocator::Allocation &stack) {
			map_local((addr_t)stack.ptr,
			          Hw::Mm::hypervisor_stack().base,
			          Hw::Mm::hypervisor_stack().size / get_page_size(),
			          Hw::PAGE_FLAGS_KERN_DATA);

			stack.deallocate = false;

			static Vm_root vm_root(ep, sh, core_ram, mapped_ram, local_rm, trace_sources);
			static Core_service<Session_object<Vm_session>> vm_service(services, vm_root);
		},
		[&] (Alloc_error) {
			warning("failed to allocate hypervisor stack for VM service");
		}
	);
}
