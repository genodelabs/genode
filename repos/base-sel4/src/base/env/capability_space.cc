/*
 * \brief  Instance of the component's local capability space
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <base/native_types.h>
#include <base/printf.h>

/* base-internal includes */
#include <internal/capability_data.h>
#include <internal/capability_space_sel4.h>


/**
 * Definition of capability meta data
 */
struct Genode::Native_capability::Data : Capability_data
{
	Data(Rpc_obj_key key) : Capability_data(key) { }

	Data() { }
};


using namespace Genode;


/**
 * Singleton instance of core-specific capability space
 */
namespace {

	struct Local_capability_space
	:
		Capability_space_sel4<8*1024, Native_capability::Data>
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

Native_capability::Data &Capability_space::create_ep_cap(Thread_base &ep_thread)
{
	unsigned const ep_sel = ep_thread.tid().ep_sel;

	return local_capability_space().create_capability(ep_sel,
	                                                  Rpc_obj_key());
}


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

