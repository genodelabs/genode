/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core-local includes */
#include <rpc_cap_factory.h>
#include <platform_pd.h>

/* NOVA includes */
#include <nova/capability_space.h>

using namespace Genode;


Native_capability Rpc_cap_factory::alloc(Native_capability ep, addr_t entry, addr_t mtd)
{
	addr_t pt_sel = cap_map()->insert();
	addr_t pd_sel = Platform_pd::pd_core_sel();
	addr_t ec_sel = ep.local_name();

	using namespace Nova;

	Lock::Guard guard(_lock);

	/* create cap object */
	Cap_object * pt_cap = new (&_slab) Cap_object(pt_sel);
	if (!pt_cap)
		return Native_capability();

	_list.insert(pt_cap);

	/* create portal */
	uint8_t const res = create_pt(pt_sel, pd_sel, ec_sel, Mtd(mtd), entry);
	if (res == NOVA_OK)
		return Capability_space::import(pt_sel);

	error("cap alloc - "
	      "cap=",   Hex(ec_sel), ":", Hex(ep.local_name()), " "
	      "entry=", Hex(entry),  " "
	      "mtd=",   Hex(mtd),    " "
	      "xpt=",   Hex(pt_sel), " "
	      "res=",   res);

	_list.remove(pt_cap);
	destroy(&_slab, pt_cap);

	/* cleanup unused selectors */
	cap_map()->remove(pt_sel, 0, false);

	return Native_capability();
}


void Rpc_cap_factory::free(Native_capability cap)
{
	if (!cap.valid()) return;

	Lock::Guard guard(_lock);

	for (Cap_object *obj = _list.first(); obj ; obj = obj->next()) {
		if (cap.local_name() == (long)obj->_cap_sel) {
			Nova::revoke(Nova::Obj_crd(obj->_cap_sel, 0));
			cap_map()->remove(obj->_cap_sel, 0, false);

			_list.remove(obj);
			destroy(&_slab, obj);
			return;
		}
	}
	warning("attempt to free invalid cap object");
}


Rpc_cap_factory::Rpc_cap_factory(Allocator &md_alloc) : _slab(&md_alloc) { }


Rpc_cap_factory::~Rpc_cap_factory()
{
	Lock::Guard guard(_lock);

	for (Cap_object *obj; (obj = _list.first()); ) {
		Nova::revoke(Nova::Obj_crd(obj->_cap_sel, 0));
		cap_map()->remove(obj->_cap_sel, 0, false);

		_list.remove(obj);
		destroy(&_slab, obj);
	}
}
