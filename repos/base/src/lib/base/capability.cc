/*
 * \brief  Capability lifetime management
 * \author Norman Feske
 * \date   2015-05-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base-internal includes */
#include <base/internal/capability_space.h>

using namespace Genode;


Native_capability::Native_capability() { }


void Native_capability::_inc()
{
	if (_data)
		Capability_space::inc_ref(*_data);
}


void Native_capability::_dec()
{
	if (_data)
		Capability_space::dec_ref(*_data);
}


long Native_capability::local_name() const
{
	return _data ? Capability_space::rpc_obj_key(*_data).value() : 0;
}


bool Native_capability::valid() const
{
	return _data != nullptr;
}


void Native_capability::print(Output &out) const
{
	using Genode::print;

	print(out, "cap<");
	if (_data) {
		Capability_space::print(out, *_data);
	} else {
		print(out, "invalid");
	}
	print(out, ">");
}

