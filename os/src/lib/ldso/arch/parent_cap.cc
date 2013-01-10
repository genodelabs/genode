/*
 * \brief  Parent capability manipulation
 * \author Sebastian Sumpf
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/child.h>
#include <ldso/arch.h>
#include <util/string.h>

using namespace Genode;

void Genode::set_parent_cap_arch(void *ptr)
{
	Genode::Parent_capability cap = parent_cap();
	memcpy(ptr, &cap, sizeof(cap));
}
