/*
 * \brief  Instance of the (Genode) capability space for non-core components
 * \author Norman Feske
 * \date   2015-05-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/capability.h>
#include <base/log.h>
#include <util/bit_allocator.h>

/* base-internal includes */
#include <base/internal/capability_data.h>
#include <base/internal/capability_space_sel4.h>

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
		Capability_space_sel4<8*1024, 1UL << NUM_CORE_MANAGED_SEL_LOG2,
		                      Native_capability::Data>
	{ };

	static Local_capability_space &local_capability_space()
	{
		static Local_capability_space capability_space;
		return capability_space;
	}
}


/*************************************************
 ** Allocator for component-local cap selectors **
 *************************************************/

namespace {

	class Sel_alloc : Bit_allocator<1UL << CSPACE_SIZE_LOG2>
	{
		private:

			Mutex _mutex { };

		public:

			Sel_alloc() { _reserve(0, 1UL << NUM_CORE_MANAGED_SEL_LOG2); }

			unsigned alloc()
			{
				Mutex::Guard guard(_mutex);
				return Bit_allocator::alloc();
			}

			void free(unsigned sel)
			{
				Mutex::Guard guard(_mutex);
				Bit_allocator::free(sel);
			}
	};

	static Sel_alloc &sel_alloc()
	{
		static Sel_alloc inst;
		return inst;
	}
}


/******************************************************
 ** Implementation of the Capability_space interface **
 ******************************************************/

Native_capability Capability_space::create_ep_cap(Thread &ep_thread)
{
	Cap_sel const ep_sel = Cap_sel(ep_thread.native_thread().ep_sel);

	Native_capability::Data *data =
		&local_capability_space().create_capability(ep_sel, Rpc_obj_key());
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


void Capability_space::print(Output &out, Native_capability::Data const &data)
{
	return local_capability_space().print(out, data);
}


Capability_space::Ipc_cap_data Capability_space::ipc_cap_data(Native_capability const &cap)
{
	return local_capability_space().ipc_cap_data(*cap.data());
}


Native_capability Capability_space::lookup(Rpc_obj_key rpc_obj_key)
{
	Native_capability::Data *data = local_capability_space().lookup(rpc_obj_key);

	return data ? Native_capability(data) : Native_capability();
}


unsigned Capability_space::alloc_rcv_sel()
{
	unsigned const rcv_sel = sel_alloc().alloc();

	seL4_SetCapReceivePath(INITIAL_SEL_CNODE, rcv_sel, CSPACE_SIZE_LOG2);

	return rcv_sel;
}


void Capability_space::reset_sel(unsigned sel)
{
	int ret = seL4_CNode_Delete(INITIAL_SEL_CNODE, sel, CSPACE_SIZE_LOG2);
	if (ret != 0)
		warning("seL4_CNode_Delete returned ", ret);
}


Native_capability Capability_space::import(Ipc_cap_data ipc_cap_data)
{
	Native_capability::Data *data =
		&local_capability_space().create_capability(ipc_cap_data.sel,
		                                            ipc_cap_data.rpc_obj_key);

	return Native_capability(data);
}
