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

#include <os/server.h>
#include <cap_session/connection.h>

using namespace Server;


static Cap_session &global_cap_session()
{
	static Cap_connection inst;
	return inst;
}


static Rpc_entrypoint &global_rpc_ep()
{
	static Rpc_entrypoint inst(&global_cap_session(),
	                           stack_size(),
	                           name());
	return inst;
}


static Entrypoint &global_ep()
{
	static Server::Entrypoint inst;
	return inst;
}


static Genode::Signal_receiver &global_sig_rec()
{
	static Genode::Signal_receiver inst;
	return inst;
}


static void dispatch(Signal &sig)
{
	Signal_dispatcher_base *dispatcher = 0;
	dispatcher = dynamic_cast<Signal_dispatcher_base *>(sig.context());

	if (!dispatcher)
		return;

	dispatcher->dispatch(sig.num());
}


/**
 * Dispatch a signal at entry point
 */
void Server::wait_and_dispatch_one_signal()
{
	Signal sig = global_sig_rec().wait_for_signal();
	dispatch(sig);
}


Signal_context_capability Entrypoint::manage(Signal_dispatcher_base &dispatcher)
{
	return global_sig_rec().manage(&dispatcher);
}


void Server::Entrypoint::dissolve(Signal_dispatcher_base &dispatcher)
{
	global_sig_rec().dissolve(&dispatcher);
}


Server::Entrypoint::Entrypoint() : _rpc_ep(global_rpc_ep()) { }


namespace Server {
	struct Constructor;
	struct Constructor_component;
}


struct Server::Constructor
{
	GENODE_RPC(Rpc_construct, void, construct);
	GENODE_RPC(Rpc_signal, void, signal);
	GENODE_RPC_INTERFACE(Rpc_construct, Rpc_signal);
};


struct Server::Constructor_component : Rpc_object<Server::Constructor,
                                                  Server::Constructor_component>
{
	void construct() { Server::construct(global_ep()); }

	void signal()
	{
		try {
			Signal sig = global_sig_rec().pending_signal();
			::dispatch(sig);
		} catch (Signal_receiver::Signal_not_pending) { }
	}
};


int main(int argc, char **argv)
{
	static Server::Constructor_component constructor;

	static Server::Entrypoint ep;

	/* call Server::construct in the context of the entrypoint */
	Capability<Server::Constructor> constructor_cap = ep.manage(constructor);

	constructor_cap.call<Server::Constructor::Rpc_construct>();

	/* process incoming signals */
	for (;;) {

		global_sig_rec().block_for_signal();

		/*
		 * It might happen that we try to forward a signal to the entrypoint,
		 * while the context of that signal is already destroyed. In that case
		 * we will get an ipc error exception as result, which has to be caught.
		 */
		try {
			constructor_cap.call<Server::Constructor::Rpc_signal>();
		} catch(Genode::Ipc_error) { }
	}
}
