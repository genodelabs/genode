/*
 * \brief  Make dataspace accessible to other Linux processes
 * \author Norman Feske
 * \date   2006-07-03
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* glibc includes */
#include <fcntl.h>

/* Genode includes */
#include <base/snprintf.h>

/* local includes */
#include <ram_dataspace_factory.h>
#include <resource_path.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* Linux syscall bindings */
#include <core_linux_syscalls.h>


using namespace Genode;


static int ram_ds_cnt = 0;  /* counter for creating unique dataspace IDs */

void Ram_dataspace_factory::_export_ram_ds(Dataspace_component *ds)
{
	char fname[Linux_dataspace::FNAME_LEN];

	/* create file using a unique file name in the resource path */
	snprintf(fname, sizeof(fname), "%s/ds-%d", resource_path(), ram_ds_cnt++);
	lx_unlink(fname);
	int const fd = lx_open(fname, O_CREAT|O_RDWR|O_TRUNC|LX_O_CLOEXEC, S_IRWXU);
	lx_ftruncate(fd, ds->size());

	/* remember file descriptor in dataspace component object */
	ds->fd(fd);

	/*
	 * Wipe the file from the Linux file system. The kernel will still keep the
	 * then unnamed file around until the last reference to the file will be
	 * gone (i.e., an open file descriptor referring to the file). A process
	 * w/o the right file descriptor won't be able to open and access the file.
	 */
	lx_unlink(fname);
}


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component *ds)
{
	int const fd = Capability_space::ipc_cap_data(ds->fd()).dst.socket;
	if (fd != -1)
		lx_close(fd);
}


void Ram_dataspace_factory::_clear_ds(Dataspace_component *ds) { }
