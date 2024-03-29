/*
 * \brief  Core implementation of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <root/root.h>

/* core includes */
#include <rom_session_component.h>

using namespace Core;


Rom_session_component::Rom_session_component(Rom_fs         &rom_fs,
                                             Rpc_entrypoint &ds_ep,
                                             const char     *args)
:
	_rom_module(&_find_rom(rom_fs, args)),
	_ds(_rom_module ? _rom_module->size : 0,
	    _rom_module ? _rom_module->addr : 0, CACHED, false, 0),
	_ds_ep(ds_ep),
	_ds_cap(static_cap_cast<Rom_dataspace>(_ds_ep.manage(&_ds)))
{ }


Rom_session_component::~Rom_session_component()
{
	_ds_ep.dissolve(&_ds);
}
