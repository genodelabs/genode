/*
 * \brief  Platform specific services for x86
 * \author Stefan Kalkowski
 * \date   2012-10-26
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/service.h>

/* core includes */
#include <core_env.h>
#include <platform.h>
#include <platform_services.h>
#include <io_port_root.h>


/*
 * Add x86 specific ioport service
 */
void Genode::platform_add_local_services(Rpc_entrypoint*,
                                         Sliced_heap *sliced_heap,
                                         Service_registry *local_services)
{
	static Io_port_root io_port_root(core_env()->cap_session(),
	                                 platform()->io_port_alloc(), sliced_heap);
	static Local_service io_port_ls(Io_port_session::service_name(),
	                                &io_port_root);
	local_services->insert(&io_port_ls);
}
