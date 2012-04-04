/*
 * \brief  ROM session component for tar files
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_SESSION_COMPONENT_H_
#define _ROM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <root/client.h>
#include <rom_session/connection.h>

class Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		Genode::Rpc_entrypoint          *_ds_ep;
		Genode::Rom_dataspace_capability _ds_cap;

		Genode::Root_client             *_tar_server_client;
		Genode::Rom_session_capability   _tar_server_session;
		Genode::Rom_connection          *_parent_rom_connection;

	public:

		/**
		 * Constructor
		 *
		 * \param ds_ep   entry point to manage the dataspace
		 *                corresponding the rom session
		 * \param args    session-construction arguments, in
		 *                particular the filename
		 */
		Rom_session_component(Genode::Rpc_entrypoint  *ds_ep,
		                      const char              *args,
		                      Genode::Root_capability  tar_server_root);

		/**
		 * Destructor
		 */
		~Rom_session_component();


		/***************************
		 ** Rom session interface **
		 ***************************/

		Genode::Rom_dataspace_capability dataspace() { return _ds_cap; }

		void sigh(Genode::Signal_context_capability) { }
};

#endif /* _ROM_SESSION_COMPONENT_H_ */
