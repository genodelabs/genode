/*
 * \brief   Platform specific services for base-hw (TrustZone)
 * \author  Stefan Kalkowski
 * \date    2012-10-26
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/service.h>
#include <drivers/trustzone.h>

/* Core includes */
#include <platform.h>
#include <platform_services.h>
#include <vm_root.h>


/*
 * Add TrustZone specific vm service
 */
void Genode::platform_add_local_services(Genode::Rpc_entrypoint *ep,
                                         Genode::Sliced_heap *sh,
                                         Genode::Service_registry *ls)
{
	using namespace Genode;

	/*
	 * We use an extra portion of RAM for the VM state,
	 * so we can map it non-cached to core instead of normal, cached RAM.
	 * In future, when core only maps memory on demand, this extra allocator,
	 * can be eliminated.
	 */
	static Synchronized_range_allocator<Allocator_avl> vm_alloc(0);
	vm_alloc.add_range(Trustzone::VM_STATE_BASE, Trustzone::VM_STATE_SIZE);
	static Vm_root vm_root(ep, sh, &vm_alloc);
	static Local_service vm_ls(Vm_session::service_name(), &vm_root);
	ls->insert(&vm_ls);
}
