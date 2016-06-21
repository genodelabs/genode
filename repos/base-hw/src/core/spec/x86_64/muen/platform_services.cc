/*
 * \brief   Platform specific services for HW kernel on Muen
 * \author  Stefan Kalkowski
 * \date    2015-06-03
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/service.h>

/* Core includes */
#include <core_env.h>
#include <platform.h>
#include <platform_services.h>
#include <vm_root.h>
#include <io_port_root.h>

/*
 * Add I/O port service and virtualization specific vm service
 */
void Genode::platform_add_local_services(Genode::Rpc_entrypoint *ep,
                                         Genode::Sliced_heap *sh,
                                         Genode::Service_registry *ls)
{
	using namespace Genode;

	static Vm_root vm_root(ep, sh);
	static Local_service vm_ls(Vm_session::service_name(), &vm_root);
	static Io_port_root io_port_root(core_env()->pd_session(),
	                                 platform()->io_port_alloc(), sh);
	static Local_service io_port_ls(Io_port_session::service_name(),
	                                &io_port_root);
	ls->insert(&vm_ls);
	ls->insert(&io_port_ls);
}
