/*
 * \brief  Implementation of the RAM session interface
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>

#include "ram_session_component.h"

using namespace Genode;


Ram_session_component::Ram_session_component(const char *args)
: _parent_ram_session(env()->parent()->session<Ram_session>(args))
{ }


Ram_session_component::~Ram_session_component()
{ }


Ram_dataspace_capability Ram_session_component::alloc(size_t ds_size, bool cached)
{
	return _parent_ram_session.alloc(ds_size);
}


void Ram_session_component::free(Ram_dataspace_capability ds_cap)
{
	_parent_ram_session.free(ds_cap);
}


int Ram_session_component::ref_account(Ram_session_capability ram_session_cap)
{
	return _parent_ram_session.ref_account(ram_session_cap);
}


int Ram_session_component::transfer_quota(Ram_session_capability ram_session_cap,
                                          size_t amount)
{
	return _parent_ram_session.transfer_quota(ram_session_cap, amount);
}


size_t Ram_session_component::quota()
{
	return _parent_ram_session.quota();
}


size_t Ram_session_component::used()
{
	return _parent_ram_session.used();
}

