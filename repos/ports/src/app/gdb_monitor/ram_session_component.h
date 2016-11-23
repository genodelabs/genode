/*
 * \brief  Core-specific instance of the RAM session interface
 * \author Norman Feske
 * \date   2006-06-19
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RAM_SESSION_COMPONENT_H_
#define _RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <ram_session/client.h>

namespace Gdb_monitor
{
	class Ram_session_component;
	using namespace Genode;
}

class Gdb_monitor::Ram_session_component : public Rpc_object<Ram_session>
{
	private:

		Genode::Env    &_env;

		Genode::Parent::Client _parent_client;
		
		Id_space<Parent::Client>::Element const _id_space_element
		{ _parent_client, _env.id_space() };

		Genode::Ram_session_client _parent_ram_session;

	public:

		/**
		 * Constructor
		 */
		Ram_session_component(Genode::Env &env, const char *args,
		                      Affinity const &affinity);

		/**
		 * Destructor
		 */
		~Ram_session_component();


		/***************************
		 ** RAM Session interface **
		 ***************************/

		Ram_dataspace_capability alloc(Genode::size_t, Cache_attribute);
		void free(Ram_dataspace_capability);
		int ref_account(Ram_session_capability);
		int transfer_quota(Ram_session_capability, Genode::size_t);
		Genode::size_t quota();
		Genode::size_t used();
};

#endif /* _RAM_SESSION_COMPONENT_H_ */
