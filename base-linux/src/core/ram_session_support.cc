/*
 * \brief  Make dataspace accessible to other Linux processes
 * \author Norman Feske
 * \date   2006-07-03
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* glibc includes */
#include <fcntl.h>

/* Genode includes */
#include <base/snprintf.h>

/* local includes */
#include "ram_session_component.h"

/* Linux syscall bindings */
#include <linux_syscalls.h>
#include <linux_rpath.h>


using namespace Genode;


static int ram_ds_cnt = 0;  /* counter for creating unique dataspace IDs */

void Ram_session_component::_export_ram_ds(Dataspace_component *ds)
{
	char fname_buf[Linux_dataspace::FNAME_LEN];

	/* assign filename to dataspace */
	snprintf(fname_buf, sizeof(fname_buf), "%s/ds-%d", lx_rpath(), ram_ds_cnt++);

	ds->fname(fname_buf);

	/* create new file representing the dataspace */
	lx_unlink(fname_buf);
	int fd = lx_open(fname_buf, O_CREAT | O_RDWR | O_TRUNC | LX_O_CLOEXEC, S_IRWXU);
	lx_ftruncate(fd, ds->size());
	lx_close(fd);
}


void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds)
{
	lx_unlink(ds->fname().buf);
}


void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	memset((void *)ds->phys_addr(), 0, ds->size());
}
