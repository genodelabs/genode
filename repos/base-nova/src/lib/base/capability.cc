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
#include <base/internal/capability_data.h>

/* NOVA includes */
#include <nova/cap_map.h>
#include <nova/capability_space.h>

using namespace Genode;


Native_capability::Native_capability()
{
	*this = Capability_space::import(Capability_space::INVALID_INDEX);
}


void Native_capability::_inc()
{
	Cap_index idx(cap_map()->find(local_name()));
	idx.inc();
}


void Native_capability::_dec()
{
	Cap_index idx(cap_map()->find(local_name()));
	idx.dec();
}


long Native_capability::local_name() const
{
	if (valid())
		return Capability_space::crd(*this).base();
	else
		return Capability_space::INVALID_INDEX;
}


bool Native_capability::valid() const
{
	return _data != nullptr;
}


Native_capability::Raw Native_capability::raw() const
{
	return { 0, 0, 0, 0 };
}


void Native_capability::print(Genode::Output &out) const
{
	using Genode::print;

	print(out, "cap<");
	if (_data) {
		print(out, local_name());
	} else {
		print(out, "invalid");
	}
	print(out, ">");
}
