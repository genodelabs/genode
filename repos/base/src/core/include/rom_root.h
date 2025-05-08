/*
 * \brief  ROM root interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ROM_ROOT_H_
#define _CORE__INCLUDE__ROM_ROOT_H_

#include <root/component.h>

/* Genode includes */
#include <rom_session_component.h>

namespace Core { struct Rom_root; }


struct Core::Rom_root : Root_component<Rom_session_component>
{
	Rom_fs         &_rom_fs;  /* rom file system */
	Rpc_entrypoint &_ds_ep;   /* entry point for managing rom dataspaces */

	Create_result _create_session(const char *args) override
	{
		return _alloc_obj(_rom_fs, _ds_ep, args);
	}

	/**
	 * Constructor
	 *
	 * \param session_ep  entry point for managing ram session objects
	 * \param ds_ep       entry point for managing dataspaces
	 * \param rom_fs      platform ROM file system
	 * \param md_alloc    meta-data allocator to be used by root component
	 */
	Rom_root(Rpc_entrypoint &session_ep,
	         Rpc_entrypoint &ds_ep,
	         Rom_fs         &rom_fs,
	         Allocator      &md_alloc)
	:
		Root_component<Rom_session_component>(&session_ep, &md_alloc),
		_rom_fs(rom_fs), _ds_ep(ds_ep)
	{ }
};

#endif /* _CORE__INCLUDE__ROM_ROOT_H_ */
