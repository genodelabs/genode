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
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Linux includes */
#include <core_linux_syscalls.h>
#include <sys/fcntl.h>

/* Genode includes */
#include <linux_dataspace/linux_dataspace.h>
#include <util/arg_string.h>
#include <root/root.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* local includes */
#include "rom_session_component.h"

using namespace Genode;


Rom_session_component::Rom_session_component(Rom_fs         *,
                                             Rpc_entrypoint *ds_ep,
                                             const char     *args)
: _ds(args), _ds_ep(ds_ep)
{
	Dataspace_capability ds_cap = _ds_ep->manage(&_ds);
	_ds_cap = static_cap_cast<Rom_dataspace>(ds_cap);
}


Rom_session_component::~Rom_session_component()
{
	_ds_ep->dissolve(&_ds);

	int const fd = Capability_space::ipc_cap_data(_ds.fd()).dst.socket;
	if (fd != -1)
		lx_close(fd);
}
