/*
 * \brief   x86-specific platform definitions
 * \author  Stefan Kalkowski
 * \date    2012-10-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>

/* core includes */
#include <platform.h>
#include <core_env.h>
#include <io_port_root.h>

using namespace Genode;

void Platform::add_local_services(Rpc_entrypoint *e, Sliced_heap *sliced_heap,
                                  Core_env *env, Service_registry *local_services)
{
	/* add x86 specific ioport service */
	static Io_port_root io_port_root(env->cap_session(), io_port_alloc(), sliced_heap);
	static Local_service io_port_ls(Io_port_session::service_name(), &io_port_root);
	local_services->insert(&io_port_ls);
}
