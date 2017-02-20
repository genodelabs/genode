/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <rpc_cap_factory.h>

long Genode::Rpc_cap_factory::_unique_id_cnt;

Genode::Lock &Genode::Rpc_cap_factory::_lock()
{
	static Lock static_lock;
	return static_lock;
}
