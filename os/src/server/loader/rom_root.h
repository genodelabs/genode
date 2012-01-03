/*
 * \brief  ROM root interface for tar files
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_ROOT_H_
#define _ROM_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* local includes */
#include "rom_session_component.h"

class Rom_root : public Genode::Root_component<Rom_session_component>
{
	private:

		/**
		 * Entry point for managing rom dataspaces
		 */
		Genode::Rpc_entrypoint *_ds_ep;

		Genode::Root_capability _tar_server_root;

	protected:

		Rom_session_component *_create_session(const char *args)
		{
			return new (md_alloc()) Rom_session_component(_ds_ep, args, _tar_server_root);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep       entry point for managing ram session objects
		 * \param ds_ep            entry point for managing dataspaces
		 * \param md_alloc         meta-data allocator to be used by root component
		 * \param tar_server_root  root capability for tar server
		 */
		Rom_root(Genode::Rpc_entrypoint  *session_ep,
		         Genode::Rpc_entrypoint  *ds_ep,
		         Genode::Allocator       *md_alloc,
		         Genode::Root_capability  tar_server_root)
		:
			Genode::Root_component<Rom_session_component>(session_ep, md_alloc),
			_ds_ep(ds_ep), _tar_server_root(tar_server_root) { }
};

#endif /* _ROM_ROOT_H_ */
