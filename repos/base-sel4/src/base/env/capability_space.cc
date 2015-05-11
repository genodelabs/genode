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
 * Singleton instance of component-local capability space
 */
namespace {

	struct Local_capability_space
	:
		Capability_space_sel4<4*1024, 1024UL, Native_capability::Data>
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

Native_capability Capability_space::create_ep_cap(Thread_base &ep_thread)
{
	unsigned const ep_sel = ep_thread.tid().ep_sel;

	Native_capability::Data &data =
		local_capability_space().create_capability(ep_sel, Rpc_obj_key());

	return Native_capability(data);
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


Capability_space::Ipc_cap_data Capability_space::ipc_cap_data(Native_capability const &cap)
{
	return local_capability_space().ipc_cap_data(*cap.data());
}


Native_capability Capability_space::lookup(Rpc_obj_key rpc_obj_key)
{
	Native_capability::Data *data = local_capability_space().lookup(rpc_obj_key);

	return data ? Native_capability(*data) : Native_capability();
}


unsigned Capability_space::alloc_rcv_sel()
{
	PDBG("not implemented");
	for (;;);
	return 0;
}


void Capability_space::reset_sel(unsigned sel)
{
	PDBG("not implemented");
}


Native_capability Capability_space::import(Ipc_cap_data ipc_cap_data)
{
	PDBG("not implemented");

	return Native_capability();
}
