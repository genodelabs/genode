/*
 * \brief  Default version of platform-specific part of RPC framework
 * \author Norman Feske
 * \date   2006-05-12
 *
 * This version is suitable for platforms similar to L4. Each platform
 * for which this implementation is not suited contains a platform-
 * specific version in its respective 'base-<platform>' repository.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/rpc_server.h>

using namespace Genode;


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	Untyped_capability ep_cap = Native_capability(_cap.dst(), 0);
	Untyped_capability new_obj_cap = _cap_session->alloc(ep_cap);

	/* add server object to object pool */
	obj->cap(new_obj_cap);
	insert(obj);

	/* return capability that uses the object id as badge */
	return new_obj_cap;
}
