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
#include <core_parent.h>
#include <vm_root.h>


/*
 * Add TrustZone specific vm service
 */
void Genode::platform_add_local_services(Rpc_entrypoint    *ep,
                                         Sliced_heap       *sliced_heap,
                                         Registry<Service> *local_services)
{
	static Vm_root                            vm_root(ep, sliced_heap);
	static Core_service<Vm_session_component> vm_service(*local_services, vm_root);
}
