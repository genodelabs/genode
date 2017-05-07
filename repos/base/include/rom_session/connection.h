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


class Genode::Rom_connection : public Connection<Rom_session>,
                               public Rom_session_client
{
	public:

		class Rom_connection_failed : public Service_denied { };

		enum { RAM_QUOTA = 6*1024UL };

	private:

		Rom_session_capability _session(Parent &parent, char const *label)
		{
			return session("ram_quota=%ld, cap_quota=%ld, label=\"%s\"",
			               RAM_QUOTA, CAP_QUOTA, label);
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
		Rom_connection(const char *label) __attribute__((deprecated))
		try :
			Connection<Rom_session>(_session(*env_deprecated()->parent(), label)),
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
		 *
		 * This version is deliberately used by functions that are marked as
		 * deprecated. If such a function called directly the
		 * __attribute__((deprecate)) version, we would always get a warning,
		 * even if the outer deprecated function is not called.
		 *
		 * It will be removed as soon as they are gone.
		 */
		Rom_connection(bool, const char *label)
		try :
			Connection<Rom_session>(_session(*env_deprecated()->parent(), label)),
			Rom_session_client(cap())
		{ }
		catch (...) {
			error("Could not open ROM session for \"", label, "\"");
			throw Rom_connection_failed();
		}
};

#endif /* _INCLUDE__ROM_SESSION__CONNECTION_H_ */
