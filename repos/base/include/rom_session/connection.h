/*
 * \brief  Connection to ROM file service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ROM_SESSION__CONNECTION_H_
#define _INCLUDE__ROM_SESSION__CONNECTION_H_

#include <rom_session/client.h>
#include <base/connection.h>
#include <base/log.h>

namespace Genode { class Rom_connection; }


struct Genode::Rom_connection : Connection<Rom_session>,
                                Rom_session_client
{
	class Rom_connection_failed : public Service_denied { };

	enum { RAM_QUOTA = 6*1024UL };

	/**
	 * Constructor
	 *
	 * \param label  request label and name of ROM module
	 *
	 * \throw Rom_connection_failed
	 */
	Rom_connection(Env &env, const char *label)
	try :
		Connection<Rom_session>(env,
		                        session(env.parent(),
		                                "ram_quota=%ld, cap_quota=%ld, label=\"%s\"",
		                                 RAM_QUOTA, CAP_QUOTA, label)),
		Rom_session_client(cap())
	{ }
	catch (...) {
		error("Could not open ROM session for \"", label, "\"");
		throw Rom_connection_failed();
	}
};

#endif /* _INCLUDE__ROM_SESSION__CONNECTION_H_ */
