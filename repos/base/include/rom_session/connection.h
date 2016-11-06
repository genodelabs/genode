/*
 * \brief  Connection to ROM file service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ROM_SESSION__CONNECTION_H_
#define _INCLUDE__ROM_SESSION__CONNECTION_H_

#include <rom_session/client.h>
#include <base/connection.h>
#include <base/log.h>

namespace Genode { class Rom_connection; }


class Genode::Rom_connection : public Connection<Rom_session>,
                               public Rom_session_client
{
	public:

		class Rom_connection_failed : public Parent::Exception { };

		enum { RAM_QUOTA = 4096UL };

	private:

		Rom_session_capability _session(Parent &parent, char const *label)
		{
			return session("ram_quota=%ld, label=\"%s\"", RAM_QUOTA, label);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param label  request label and name of ROM module
		 *
		 * \throw Rom_connection_failed
		 */
		Rom_connection(Env &env, const char *label)
		try :
			Connection<Rom_session>(env, _session(env.parent(), label)),
			Rom_session_client(cap())
		{ }
		catch (...) {
			error("Could not open ROM session for \"", label, "\"");
			throw Rom_connection_failed();
		}

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Rom_connection(const char *label)
		try :
			Connection<Rom_session>(_session(*env()->parent(), label)),
			Rom_session_client(cap())
		{ }
		catch (...) {
			error("Could not open ROM session for \"", label, "\"");
			throw Rom_connection_failed();
		}
};

#endif /* _INCLUDE__ROM_SESSION__CONNECTION_H_ */
