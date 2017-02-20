/*
 * \brief  libc addrinfo plugin wrappers
 * \author Christian Helmuth
 * \date   2017-02-08
 *
 * Note, these functions are implemented by the libc_resolv library currently
 * and can be removed when the library is merged into libc.lib.so.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <libc-plugin/plugin_registry.h>


using namespace Libc;


extern "C" void freeaddrinfo(struct addrinfo *res)
{
	Plugin *plugin;

	plugin = plugin_registry()->get_plugin_for_freeaddrinfo(res);

	if (!plugin) {
		Genode::error("no plugin found for freeaddrinfo()");
		return;
	}

	plugin->freeaddrinfo(res);
}


extern "C" int getaddrinfo(const char *node, const char *service,
                           const struct addrinfo *hints,
                           struct addrinfo **res)
{
	Plugin *plugin;

	plugin = plugin_registry()->get_plugin_for_getaddrinfo(node, service, hints, res);

	if (!plugin) {
		Genode::error("no plugin found for getaddrinfo()");
		return -1;
	}

	return plugin->getaddrinfo(node, service, hints, res);
}

