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

namespace Genode {

	class Rom_connection : public Connection<Rom_session>,
	                       public Rom_session_client
	{
		public:

			class Rom_connection_failed : public Parent::Exception { };

		private:

			Rom_session_capability _create_session(const char *filename, const char *label)
			{
				try {
					return session("ram_quota=4K, filename=\"%s\", label=\"%s\"",
					               filename, label ? label: filename); }
				catch (...) {
					PERR("Could not open file \"%s\"", filename);
					throw Rom_connection_failed();
				}
			}

		public:

			/**
			 * Constructor
			 *
			 * \param filename  name of ROM file
			 * \param label     initial session label
			 *
			 * \throw Rom_connection_failed
			 */
			Rom_connection(const char *filename, const char *label = 0) :
				Connection<Rom_session>(_create_session(filename, label)),
				Rom_session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__ROM_SESSION__CONNECTION_H_ */
