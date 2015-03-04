/*
 * \brief  Connection to ROM file service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ROM_SESSION__CONNECTION_H_
#define _INCLUDE__ROM_SESSION__CONNECTION_H_

#include <rom_session/client.h>
#include <base/connection.h>
#include <base/printf.h>

namespace Genode { class Rom_connection; }


class Genode::Rom_connection : public Connection<Rom_session>,
                               public Rom_session_client
{
	public:

		class Rom_connection_failed : public Parent::Exception { };

	private:

		Rom_session_capability _create_session(const char *module_name, const char *label)
		{
			try {
				return session("ram_quota=4K, filename=\"%s\", label=\"%s\"",
				               module_name, label ? label: module_name); }
			catch (...) {
				PERR("Could not open ROM session for module \"%s\"", module_name);
				throw Rom_connection_failed();
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param module_name  name of ROM module
		 * \param label        initial session label
		 *
		 * \throw Rom_connection_failed
		 */
		Rom_connection(const char *module_name, const char *label = 0)
		:
			Connection<Rom_session>(_create_session(module_name, label)),
			Rom_session_client(cap())
		{ }
};

#endif /* _INCLUDE__ROM_SESSION__CONNECTION_H_ */
