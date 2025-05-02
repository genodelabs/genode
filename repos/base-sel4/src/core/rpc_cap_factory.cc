/*
 * \brief  seL4-specific RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/capability.h>
#include <util/misc_math.h>

/* core includes */
#include <rpc_cap_factory.h>
#include <platform.h>

/* base-internal include */
#include <core_capability_space.h>

using namespace Core;


Rpc_cap_factory::Alloc_result Rpc_cap_factory::alloc(Native_capability ep)
{
	static unsigned unique_id_cnt;

	if (!ep.valid())
		return Native_capability();

	Mutex::Guard guard(_mutex);

	Rpc_obj_key const rpc_obj_key(++unique_id_cnt);

	auto cap = Capability_space::create_rpc_obj_cap(ep, rpc_obj_key);

	try {
		if (cap.valid()) {
			_pool.insert(new (_entry_slab) Entry(cap));
			return cap;
		}
	}
	catch (Out_of_caps) { return Alloc_error::OUT_OF_CAPS; }
	catch (Out_of_ram)  { return Alloc_error::OUT_OF_RAM;  }

	return Alloc_error::DENIED;
}


void Rpc_cap_factory::free(Native_capability cap)
{
	if (!cap.valid())
		return;

	Mutex::Guard guard(_mutex);

	Entry * entry_ptr = nullptr;
	_pool.apply(cap, [&] (Entry * ptr) {
		if (!ptr)
			return;

		_pool.remove(ptr);

		Capability_space::destroy_rpc_obj_cap(cap);

		entry_ptr = ptr;
	});

	if (entry_ptr)
		destroy(_entry_slab, entry_ptr);
}


Rpc_cap_factory::~Rpc_cap_factory()
{
	Mutex::Guard guard(_mutex);

	_pool.remove_all([this] (Entry *ptr) {
		if (ptr)
			destroy(_entry_slab, ptr); });
}
