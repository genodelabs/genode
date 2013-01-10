/*
 * \brief  Retrieve progam name from dataspace (Linux specific)
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-01-04
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <ldso/arch.h>

using namespace Genode;
/*
 * Function is implemented for Linux/GDB only 
 */
int Genode::binary_name(Dataspace_capability ds_cap, char *buf, size_t buf_size)
{
	return -1;
}
