/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <rpc_cap_factory.h>

using namespace Genode;


Native_capability Rpc_cap_factory::alloc(Native_capability)
{
	return Native_capability();
}


void Rpc_cap_factory::free(Native_capability) { }

