/*
 * \brief  Connection to CAP service
 * \author Norman Feske
 * \date   2008-08-22
 *
 * \deprecated
 *
 * This header is scheduled for removal. It exists for API compatiblity only.
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CAP_SESSION__CONNECTION_H_
#define _INCLUDE__CAP_SESSION__CONNECTION_H_

#include <cap_session/cap_session.h>
#include <pd_session/client.h>
#include <base/env.h>

namespace Genode { struct Cap_connection; }


/*
 * There are no CAP connections anymore. The only situation where CAP
 * connections were created was inside old-fashioned servers that used
 * to create an 'Rpc_entrypoint' manually. The 'Rpc_entrypoint' requires
 * a CAP session as constructor argument. We accommodate this use case
 * by allocating RPC capabilities from the server's protection domain.
 *
 * Modern components no longer create 'Rpc_entrypoint' objects directly
 * but instead use the new 'Entrypoint' interface.
 */
struct Genode::Cap_connection : Pd_session_client
{
	Cap_connection() : Pd_session_client(env()->pd_session_cap()) { }
};

#endif /* _INCLUDE__CAP_SESSION__CONNECTION_H_ */
