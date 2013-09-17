/*
 * \brief  Dataspace abstraction between Genode and L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>

/* L4lx includes */
#include <dataspace.h>


L4lx::Dataspace* L4lx::Dataspace_tree::insert(const char* name,
                                              Genode::Dataspace_capability cap)
{
	using namespace L4lx;

	Genode::Dataspace_client dsc(cap);
	Dataspace *ds =
		new (Genode::env()->heap()) Single_dataspace(name, dsc.size(), cap);
	insert(ds);
	return ds;
}
