/*
 * \brief  Core-specific instance of the RAM session interface
 * \author Norman Feske
 * \date   2006-06-19
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RAM_SESSION_COMPONENT_H_
#define _RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <ram_session/client.h>

using namespace Genode;

class Ram_session_component : public Rpc_object<Ram_session>
{
	private:

		Genode::Ram_session_client _parent_ram_session;

	public:

		/**
		 * Constructor
		 */
		Ram_session_component(const char *args);

		/**
		 * Destructor
		 */
		~Ram_session_component();


		/***************************
		 ** RAM Session interface **
		 ***************************/

		Ram_dataspace_capability alloc(Genode::size_t, bool);
		void free(Ram_dataspace_capability);
		int ref_account(Ram_session_capability);
		int transfer_quota(Ram_session_capability, Genode::size_t);
		Genode::size_t quota();
		Genode::size_t used();
};

#endif /* _RAM_SESSION_COMPONENT_H_ */
