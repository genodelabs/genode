/*
 * \brief  Capability lifetime management
 * \author Norman Feske
 * \date   2015-05-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

/* base-internal includes */
#include <base/internal/cap_map.h>
#include <base/internal/capability_data.h>

using namespace Genode;


Native_capability::Native_capability() { }


void Native_capability::_inc()
{
	if (_data)
		_data->inc();
}


void Native_capability::_dec()
{
	if (_data && !_data->dec())
		cap_map()->remove(_data);
}


long Native_capability::local_name() const
{
	return _data ? _data->id() : 0;
}


bool Native_capability::valid() const
{
	return (_data != 0) && _data->valid();
}


Native_capability::Raw Native_capability::raw() const
{
	return { (unsigned long)local_name(), 0, 0, 0 };
}


void Native_capability::print(Genode::Output &out) const
{
	using Genode::print;

	print(out, "cap<");
	if (_data) {
		print(out, "kcap=", Hex(_data->kcap()), ",key=", (unsigned)_data->id());
	} else {
		print(out, "invalid");
	}
	print(out, ">");
}
