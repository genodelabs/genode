/*
 * \brief  Connection to Report service
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REPORT_SESSION__CONNECTION_H_
#define _INCLUDE__REPORT_SESSION__CONNECTION_H_

#include <report_session/client.h>
#include <base/connection.h>

namespace Report { struct Connection; }


struct Report::Connection : Genode::Connection<Session>, Session_client
{
	enum { RAM_QUOTA = 6*4096 }; /* value used for 'Slave::Connection' */

	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Genode::Capability<Report::Session>
	_session(Genode::Parent &parent, char const *label, size_t buffer_size)
	{
		return session(parent, "label=\"%s\", ram_quota=%ld, cap_quota=%ld, buffer_size=%zd",
		               label, 10*1024 + buffer_size, CAP_QUOTA, buffer_size);
	}

	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, char const *label, size_t buffer_size = 4096)
	:
		Genode::Connection<Session>(env, _session(env.parent(), label, buffer_size)),
		Session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(char const *label, size_t buffer_size = 4096) __attribute__((deprecated))
	:
		Genode::Connection<Session>(_session(*Genode::env_deprecated()->parent(), label, buffer_size)),
		Session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \deprecated
	 * \noapi
	 *
	 * This variant solely exists to be called by deprecated functions. It
	 * will be removed as soon as those functions are gone.
	 */
	Connection(bool, char const *label, size_t buffer_size = 4096)
	:
		Genode::Connection<Session>(_session(*Genode::env_deprecated()->parent(), label, buffer_size)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__REPORT_SESSION__CONNECTION_H_ */
