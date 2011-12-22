/*
 * \brief  ROM session component for tar files
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/service.h>
#include <dataspace/client.h>
#include <util/arg_string.h>
#include <rom_session/client.h>
#include <rom_session_component.h>
#include <rom_session/connection.h>


using namespace Genode;

Rom_session_component::Rom_session_component(Rpc_entrypoint  *ds_ep,
                                             const char      *args,
                                             Root_capability  tar_server_root)
: _ds_ep(ds_ep),
  _tar_server_client(0),
  _parent_rom_connection(0)
{
	/* extract filename from session arguments */
	char fname_buf[32];
	Arg_string::find_arg(args, "filename").string(fname_buf, sizeof(fname_buf), "");

	PDBG("filename = %s", fname_buf);

	if (tar_server_root.valid()) {
		/* try to get the file from the tar server */
		_tar_server_client = new (env()->heap()) Root_client(tar_server_root);
		char tar_args[256];
		snprintf(tar_args, sizeof(tar_args), "filename=\"%s\", ram_quota=4K", fname_buf);
		_tar_server_session = static_cap_cast<Rom_session>(_tar_server_client->session(tar_args));
		Rom_session_client rsc(_tar_server_session);
		_ds_cap = rsc.dataspace();
	}

	if (!_ds_cap.valid()) {
		/* no tar server started or file not found in tar archive -> ask parent */
		PDBG("file not found in tar archive, asking parent");
		try {
			_parent_rom_connection = new (env()->heap()) Rom_connection(fname_buf);
			_ds_cap = _parent_rom_connection->dataspace();
		} catch (Rom_connection::Rom_connection_failed) {
			PDBG("could not find file %s", fname_buf);
			throw Service::Invalid_args();
		}
	}
}


Rom_session_component::~Rom_session_component()
{
	if (_tar_server_session.valid())
		_tar_server_client->close(_tar_server_session);

	destroy(env()->heap(), _tar_server_client);
	destroy(env()->heap(), _parent_rom_connection);
}
