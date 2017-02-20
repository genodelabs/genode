/*
 * \brief  Implementation of platform-specific capabilities for core
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/capability_space.h>

using namespace Genode;


Native_capability::Native_capability() { }


void Native_capability::_inc() { }
void Native_capability::_dec() { }


long Native_capability::local_name() const
{
	return (long)_data;
}


bool Native_capability::valid() const
{
	return (addr_t)_data != Kernel::cap_id_invalid();
}


Native_capability::Raw Native_capability::raw() const { return { 0, 0, 0, 0 }; }
