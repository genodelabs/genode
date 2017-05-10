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
#include <drivers/trustzone.h>

/* Core includes */
#include <platform.h>
#include <platform_services.h>
#include <core_service.h>
#include <vm_root.h>
#include <map_local.h>

extern int _mon_kernel_entry;

/*
 * Add TrustZone specific vm service
 */
void Genode::platform_add_local_services(Rpc_entrypoint    *ep,
                                         Sliced_heap       *sliced_heap,
                                         Registry<Service> *local_services)
{
	static addr_t const phys_base =
		Platform::core_phys_addr((addr_t)&_mon_kernel_entry);
	map_local(phys_base, 0xfff00000, 1); // FIXME
	static Vm_root                            vm_root(ep, sliced_heap);
	static Core_service<Vm_session_component> vm_service(*local_services, vm_root);
}
