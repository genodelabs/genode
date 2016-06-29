/*
 * \brief  Instance of the core-local (Genode) capability space
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
#include <base/capability.h>
#include <base/printf.h>

/* base-internal includes */
#include <base/internal/capability_data.h>
#include <base/internal/capability_space_sel4.h>

/* core includes */
#include <core_capability_space.h>
#include <platform.h>


/**
 * Core-specific supplement of the capability meta data
 */
class Genode::Native_capability::Data : public Capability_data
{
	private:

		Pd_session const *_pd_session = nullptr;

	public:

		Data(Pd_session const *pd_session, Rpc_obj_key key)
		:
			Capability_data(key), _pd_session(pd_session)
		{ }

		Data() { }

		bool belongs_to(Pd_session const *session) const
		{
			return _pd_session == session;
		}
};


using namespace Genode;


/**
 * Singleton instance of core-specific capability space
 */
namespace {

	struct Local_capability_space
	:
		Capability_space_sel4<1UL << Core_cspace::NUM_CORE_SEL_LOG2, 0UL,
		                      Native_capability::Data>
	{ };

	static Local_capability_space &local_capability_space()
	{
		static Local_capability_space capability_space;
		return capability_space;
	}
}


/********************************************************************
 ** Implementation of the core-specific Capability_space interface **
 ********************************************************************/

Native_capability
Capability_space::create_rpc_obj_cap(Native_capability ep_cap,
                                     Pd_session const *pd_session,
                                     Rpc_obj_key rpc_obj_key)
{
	/* allocate core-local selector for RPC object */
	Cap_sel const rpc_obj_sel = platform_specific()->core_sel_alloc().alloc();

	/* create Genode capability */
	Native_capability::Data &data =
		local_capability_space().create_capability(rpc_obj_sel, pd_session,
		                                           rpc_obj_key);

	ASSERT(ep_cap.valid());

	Cap_sel const ep_sel(local_capability_space().sel(*ep_cap.data()));

	/* mint endpoint capability into RPC object capability */
	{
		seL4_CNode     const service    = seL4_CapInitThreadCNode;
		seL4_Word      const dest_index = rpc_obj_sel.value();
		uint8_t        const dest_depth = 32;
		seL4_CNode     const src_root   = seL4_CapInitThreadCNode;
		seL4_Word      const src_index  = ep_sel.value();
		uint8_t        const src_depth  = 32;
		seL4_CapRights const rights     = seL4_AllRights;
		seL4_CapData_t const badge      = seL4_CapData_Badge_new(rpc_obj_key.value());

		int const ret = seL4_CNode_Mint(service,
		                                dest_index,
		                                dest_depth,
		                                src_root,
		                                src_index,
		                                src_depth,
		                                rights,
		                                badge);
		ASSERT(ret == seL4_NoError);
	}

	return Native_capability(data);
}


/******************************************************
 ** Implementation of the Capability_space interface **
 ******************************************************/

Native_capability Capability_space::create_ep_cap(Thread &ep_thread)
{
	Cap_sel const ep_sel(ep_thread.native_thread().ep_sel);

	/* entrypoint capabilities are not allocated from a PD session */
	Pd_session const *pd_session = nullptr;

	Native_capability::Data &data =
		local_capability_space().create_capability(ep_sel, pd_session,
		                                           Rpc_obj_key());

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
	return platform_specific()->alloc_core_rcv_sel();
}


void Capability_space::reset_sel(unsigned sel)
{
	return platform_specific()->reset_sel(sel);
}


Native_capability Capability_space::import(Ipc_cap_data ipc_cap_data)
{
	/* imported capabilities are not associated with a PD session */
	Pd_session const *pd_session = nullptr;

	Native_capability::Data &data =
		local_capability_space().create_capability(ipc_cap_data.sel, pd_session,
		                                           ipc_cap_data.rpc_obj_key);

	return Native_capability(data);
}


Native_capability
Capability_space::create_notification_cap(Cap_sel &notify_cap)
{
	Pd_session const *pd_session = nullptr;

	Native_capability::Data &data =
		local_capability_space().create_capability(notify_cap, pd_session,
		                                           Rpc_obj_key());

	return Native_capability(data);
}
