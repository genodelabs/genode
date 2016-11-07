/*
 * \brief  Instance of the (Genode) capability space for non-core components
 * \author Norman Feske
 * \date   2015-05-11
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <base/capability.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>


/**
 * Definition of capability meta data
 */
struct Genode::Native_capability::Data : Capability_data
{
	Rpc_destination dst = invalid_rpc_destination();

	Data(Rpc_destination dst, Rpc_obj_key key)
	: Capability_data(key), dst(dst) { }

	Data() { }
};


using namespace Genode;


/**
 * Singleton instance of component-local capability space
 */
namespace {

	struct Local_capability_space
	:
		Capability_space_tpl<4*1024, Native_capability::Data>
	{ };

	static Local_capability_space &local_capability_space()
	{
		static Local_capability_space capability_space;
		return capability_space;
	}
}


/******************************************************
 ** Implementation of the Capability_space interface **
 ******************************************************/

void Capability_space::dec_ref(Native_capability::Data &data)
{
	local_capability_space().dec_ref(data);
}


void Capability_space::inc_ref(Native_capability::Data &data)
{
	local_capability_space().inc_ref(data);
}


Rpc_obj_key Capability_space::rpc_obj_key(Native_capability::Data const &data)
{
	return local_capability_space().rpc_obj_key(data);
}


Capability_space::Ipc_cap_data Capability_space::ipc_cap_data(Native_capability const &cap)
{
	return local_capability_space().ipc_cap_data(*cap.data());
}


Native_capability Capability_space::lookup(Rpc_obj_key rpc_obj_key)
{
	return local_capability_space().lookup(rpc_obj_key);
}


Native_capability Capability_space::import(Rpc_destination dst, Rpc_obj_key key)
{
	return local_capability_space().import(dst, key);
}


void Capability_space::print(Output &out, Native_capability::Data const &data)
{
	local_capability_space().print(out, data);
}
