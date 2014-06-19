/*
 * \brief  Linux-specific core implementation of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 *
 * The Linux version of ROM session component does not use the
 * Rom_fs as provided as constructor argument. Instead, we map
 * rom modules directly to files of the host file system.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <core_linux_syscalls.h>
#include <sys/fcntl.h>

/* Genode includes */
#include <linux_dataspace/linux_dataspace.h>
#include <util/arg_string.h>
#include <root/root.h>

/* local includes */
#include "rom_session_component.h"

using namespace Genode;


static Genode::size_t file_size(const char *path)
{
	struct stat64 s;
	if (lx_stat(path, &s) < 0)
		return 0;
	else
		return s.st_size;
}


Rom_session_component::Rom_session_component(Rom_fs         *rom_fs,
                                             Rpc_entrypoint *ds_ep,
                                             const char     *args)
: _ds_ep(ds_ep)
{
	/* extract filename from session arguments */
	char fname[Linux_dataspace::FNAME_LEN];
	Arg_string::find_arg(args, "filename").string(fname, sizeof(fname), "");

	/* only files inside the current working directory are allowed */
	for (const char *c = fname; *c; c++)
		if (*c == '/')
			throw Root::Invalid_args();

	Genode::size_t const fsize = file_size(fname);

	/* use invalid capability as default value */
	_ds_cap = Rom_dataspace_capability();

	/* ROM module not found */
	if (fsize == 0)
		throw Root::Invalid_args();

	int const fd = lx_open(fname, O_RDONLY | LX_O_CLOEXEC, S_IRUSR | S_IXUSR);

	_ds = Dataspace_component(fsize, 0, CACHED, false, 0);
	_ds.fd(fd);
	_ds.fname(fname);

	Dataspace_capability ds_cap = _ds_ep->manage(&_ds);
	_ds_cap = static_cap_cast<Rom_dataspace>(ds_cap);
}


Rom_session_component::~Rom_session_component()
{
	_ds_ep->dissolve(&_ds);

	int const fd = _ds.fd().dst().socket;
	if (fd != -1)
		lx_close(fd);
}
