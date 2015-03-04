/*
 * \brief  Skeleton for implementing servers
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__SERVER_H_
#define _INCLUDE__OS__SERVER_H_

#include <os/signal_rpc_dispatcher.h>

namespace Server {

	using namespace Genode;
	class Entrypoint;


	void wait_and_dispatch_one_signal();


	/***********************************************************
	 ** Functions to be provided by the server implementation **
	 ***********************************************************/

	size_t stack_size();

	char const *name();

	void construct(Entrypoint &);
}


class Server::Entrypoint
{
	private:

		Rpc_entrypoint &_rpc_ep;

	public:

		Entrypoint();

		/**
		 * Associate RPC object with the entry point
		 */
		template <typename RPC_INTERFACE, typename RPC_SERVER>
		Capability<RPC_INTERFACE>
		manage(Rpc_object<RPC_INTERFACE, RPC_SERVER> &obj)
		{
			return _rpc_ep.manage(&obj);
		}

		/**
		 * Dissolve RPC object from entry point
		 */
		template <typename RPC_INTERFACE, typename RPC_SERVER>
		void dissolve(Rpc_object<RPC_INTERFACE, RPC_SERVER> &obj)
		{
			_rpc_ep.dissolve(&obj);
		}

		/**
		 * Associate signal dispatcher with entry point
		 */
		Signal_context_capability manage(Signal_rpc_dispatcher_base &);

		/**
		 * Disassociate signal dispatcher from entry point
		 */
		void dissolve(Signal_rpc_dispatcher_base &);

		/**
		 * Return RPC entrypoint
		 */
		Rpc_entrypoint &rpc_ep() { return _rpc_ep; }
};

#endif /* _INCLUDE__OS__SERVER_H_ */
