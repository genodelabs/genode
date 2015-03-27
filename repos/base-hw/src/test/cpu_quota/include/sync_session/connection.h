/*
 * \brief  Connection to Sync service
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYNC_SESSION__CONNECTION_H_
#define _SYNC_SESSION__CONNECTION_H_

/* Genode includes */
#include <base/connection.h>

/* local includes */
#include <sync_session/client.h>

namespace Sync
{
	using Genode::Parent;

	class Connection;
}


class Sync::Connection : public Genode::Connection<Session>,
                         public Session_client
{
	public:

		class Connection_failed : public Parent::Exception { };

	private:

		Session_capability _create_session()
		{
			try { return session("ram_quota=4K"); }
			catch (...) { throw Connection_failed(); }
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Connection_failed
		 */
		Connection() :
			Genode::Connection<Session>(_create_session()),
			Session_client(cap())
		{ }
};

#endif /* _SYNC_SESSION__CONNECTION_H_ */
