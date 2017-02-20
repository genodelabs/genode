/*
 * \brief  Utility for implementing a local service with a single session
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__SINGLE_SESSION_SERVICE_H_
#define _INCLUDE__OS__SINGLE_SESSION_SERVICE_H_

/* Genode includes */
#include <base/service.h>


namespace Genode { template <typename> class Single_session_service; }


template <typename SESSION>
class Genode::Single_session_service
{
	public:

		typedef Capability<SESSION> Session_capability;

	private:

		/*
		 * Wrap client object to be compabile with 'Rpc_object::cap' calls
		 *
		 * We hand out the capability via 'cap' method to be compatible with
		 * the interface normally provided by server-side component objects.
		 * The 'Single_session_factory' requests the capability via this
		 * method.
		 */
		struct Client : SESSION::Client
		{
			Client(Session_capability cap)                 : SESSION::Client(cap) { }
			Client(Region_map &rm, Session_capability cap) : SESSION::Client(rm, cap) { }
			Session_capability cap() const { return *this; }
		};

		typedef Local_service<Client>            Service;
		typedef typename Service::Single_session_factory Factory;

		Client  _client;
		Factory _factory { _client };
		Service _service { _factory };

	public:

		Single_session_service(Session_capability cap) : _client(cap) { }

		Single_session_service(Region_map &rm, Session_capability cap)
		: _client(rm, cap) { }

		Service &service() { return _service; }
};

#endif /* _INCLUDE__OS__SINGLE_SESSION_SERVICE_H_ */
