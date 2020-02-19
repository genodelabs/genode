/*
 * \brief  Default version of platform-specific part of RPC framework
 * \author Stefan Thöni
 * \date   2020-01-30
 */

/*
 * Copyright (C) 2006-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/rpc_server.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>

using namespace Genode;


class Rpc_entrypoint::Native_context
{
};


void Rpc_entrypoint::entry()
{
	Native_context context { };
	_entry(context);
}


size_t Rpc_entrypoint::_native_stack_size(size_t stack_size)
{
	return stack_size + sizeof(Native_context);
}

