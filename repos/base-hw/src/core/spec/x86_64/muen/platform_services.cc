/*
 * \brief   Platform specific services for HW kernel on Muen
 * \author  Stefan Kalkowski
 * \date    2015-06-03
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>

/* core includes */
#include <core_env.h>
#include <platform.h>
#include <platform_services.h>
#include <vm_root.h>
#include <io_port_root.h>

/*
 * Add I/O port service and virtualization specific vm service
 */
void Genode::platform_add_local_services(Rpc_entrypoint *ep,
                                         Sliced_heap *sh,
                                         Registry<Service> *services)
{
	static Vm_root vm_root(ep, sh);
	static Core_service<Vm_session_component> vm_ls(*services, vm_root);

	static Io_port_root io_port_root(core_env()->pd_session(),
	                                 platform()->io_port_alloc(), sh);
	static Core_service<Io_port_session_component> io_port_ls(*services,
	                                                          io_port_root);
}
