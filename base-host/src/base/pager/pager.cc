/*
 * \brief  Dummy pager framework
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/pager.h>

using namespace Genode;


/**********************
 ** Pager activation **
 **********************/

void Pager_activation_base::entry()
{
	while (1);
}


/**********************
 ** Pager entrypoint **
 **********************/

Pager_entrypoint::Pager_entrypoint(Cap_session *, Pager_activation_base *a)
: _activation(a)
{ _activation->ep(this); }


void Pager_entrypoint::dissolve(Pager_object *obj)
{
	remove(obj);
}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	/* return invalid capability if no activation is present */
	if (!_activation) return Pager_capability();

	Native_capability cap = Native_capability(_activation->cap().tid(), obj->badge());

	/* add server object to object pool */
	obj->cap(cap);
	insert(obj);

	/* return capability that uses the object id as badge */
	return Pager_capability(cap);
}
