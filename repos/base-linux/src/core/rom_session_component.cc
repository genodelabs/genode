/*
 * \brief  Linux-specific core implementation of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 *
 * The Linux version does not use the Rom_fs as provided as constructor
 * argument. Instead, ROM modules refer to files on the host file system.
 */

/*
 * Copyright (C) 2006-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <rom_session_component.h>

using namespace Core;


Rom_session_component::Rom_session_component(Rom_fs         &,
                                             Rpc_entrypoint &ds_ep,
                                             const char     *args)
{
	_ds.construct(ds_ep, args);

	if (_ds.constructed() && !_ds->_ds.fd().valid())
		_ds.destruct();
}
