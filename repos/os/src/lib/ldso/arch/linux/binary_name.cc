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
#include <linux_dataspace/client.h>

using namespace Genode;

int Genode::binary_name(Dataspace_capability ds_cap, char *buf, size_t buf_size)
{
	/* determine name of binary to start */
	Linux_dataspace_client ds_client(ds_cap);
	Linux_dataspace::Filename filename = ds_client.fname();
	strncpy(buf, filename.buf, buf_size);
	return 0;
}
