/*
 * \brief  ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 *
 * A ROM session corresponds to an open file. The file name is specified as an
 * argument on session creation.
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ROM_SESSION__ROM_SESSION_H_
#define _INCLUDE__ROM_SESSION__ROM_SESSION_H_

#include <dataspace/capability.h>
#include <session/session.h>

namespace Genode {

	struct Rom_dataspace : Dataspace { };

	typedef Capability<Rom_dataspace> Rom_dataspace_capability;

	struct Rom_session : Session
	{
		static const char *service_name() { return "ROM"; }

		virtual ~Rom_session() { }

		/**
		 * Request dataspace containing the ROM session data
		 *
		 * \return  capability to ROM dataspace
		 *
		 * The capability may be invalid.
		 */
		virtual Rom_dataspace_capability dataspace() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_dataspace, Rom_dataspace_capability, dataspace);

		GENODE_RPC_INTERFACE(Rpc_dataspace);
	};
}

#endif /* _INCLUDE__ROM_SESSION__ROM_SESSION_H_ */
