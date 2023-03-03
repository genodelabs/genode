/*
 * \brief  Connection to ROM file service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ROM_SESSION__CONNECTION_H_
#define _INCLUDE__ROM_SESSION__CONNECTION_H_

#include <rom_session/client.h>
#include <base/connection.h>

namespace Genode { class Rom_connection; }


struct Genode::Rom_connection : Connection<Rom_session>, Rom_session_client
{
	struct Rom_connection_failed : Service_denied { };

	/**
	 * Constructor
	 *
	 * \param label  name of requested ROM module
	 *
	 * \throw Rom_connection_failed
	 */
	Rom_connection(Env &env, Session_label const &label)
	try :
		Connection<Rom_session>(env, label, Ram_quota { RAM_QUOTA }, Args()),
		Rom_session_client(cap())
	{ }
	catch (...) {
		error("could not open ROM session for \"", label, "\"");
		throw Rom_connection_failed();
	}
};

#endif /* _INCLUDE__ROM_SESSION__CONNECTION_H_ */
