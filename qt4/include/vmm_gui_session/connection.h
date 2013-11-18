/*
 * \brief  Connection to VMM GUI service
 * \author Stefan Kalkowski
 * \date   2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM_GUI_SESSION__CONNECTION_H_
#define _INCLUDE__VMM_GUI_SESSION__CONNECTION_H_

#include <vmm_gui_session/client.h>
#include <base/connection.h>

namespace Vmm_gui
{
	/**
	 * Connection to VMM GUI service
	 */
	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection()
		: Genode::Connection<Session>(session("ram_quota=16K")),
		  Session_client(cap()) { }
	};
}

#endif /* _INCLUDE__VMM_GUI_SESSION__CONNECTION_H_ */
