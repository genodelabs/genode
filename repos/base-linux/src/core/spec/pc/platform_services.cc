/*
 * \brief  Platform specific services for Linux
 * \author Johannes Kliemann
 * \date   2017-11-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
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
#include <io_port_root.h>
#include <core_linux_syscalls.h>

using namespace Core;


void Core::platform_add_local_services(Rpc_entrypoint         &,
                                       Sliced_heap            &md,
                                       Registry<Service>      &services,
                                       Trace::Source_registry &,
                                       Ram_allocator          &,
                                       Region_map             &,
                                       Range_allocator        &io_port_ranges)
{
	if (!lx_iopl(3)) {
		static Io_port_root io_port_root(io_port_ranges, md);

		static Core_service<Io_port_session_component> io_port_ls(services, io_port_root);
	}
}
