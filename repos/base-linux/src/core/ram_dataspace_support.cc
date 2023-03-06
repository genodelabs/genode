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

/* local includes */
#include <ram_dataspace_factory.h>
#include <resource_path.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* Linux syscall bindings */
#include <core_linux_syscalls.h>


using namespace Core;


static int ram_ds_cnt = 0;  /* counter for creating unique dataspace IDs */

void Ram_dataspace_factory::_export_ram_ds(Dataspace_component &ds)
{
	Linux_dataspace::Filename const fname(resource_path(), "/ds-", ram_ds_cnt++);

	/* create file using a unique file name in the resource path */
	lx_unlink(fname.string());
	int const fd = lx_open(fname.string(), O_CREAT|O_RDWR|O_TRUNC|LX_O_CLOEXEC, S_IRWXU);
	lx_ftruncate(fd, ds.size());

	/* remember file descriptor in dataspace component object */
	ds.fd(fd);

	/*
	 * Wipe the file from the Linux file system. The kernel will still keep the
	 * then unnamed file around until the last reference to the file will be
	 * gone (i.e., an open file descriptor referring to the file). A process
	 * w/o the right file descriptor won't be able to open and access the file.
	 */
	lx_unlink(fname.string());
}


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component &) { }


void Ram_dataspace_factory::_clear_ds(Dataspace_component &) { }
