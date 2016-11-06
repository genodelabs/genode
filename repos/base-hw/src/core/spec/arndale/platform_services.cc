/*
 * \brief   Platform specific services for Arndale
 * \author  Stefan Kalkowski
 * \date    2014-07-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/service.h>
#include <base/heap.h>

/* core includes */
#include <platform_services.h>
#include <core_parent.h>        /* for 'Core_service' type */
#include <vm_root.h>


/*
 * Add ARM virtualization specific vm service
 */
void Genode::platform_add_local_services(Rpc_entrypoint *ep,
                                         Sliced_heap *sh,
                                         Registry<Service> *services)
{
	using namespace Genode;

	static Vm_root vm_root(ep, sh);
	static Core_service<Vm_session_component> vm_service(*services, vm_root);
}
