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


static void wait_and_dispatch_one_signal(bool entrypoint)
{
	/*
	 * We call the signal dispatcher outside of the scope of 'Signal'
	 * object because we block the RPC interface in the input handler
	 * when the kill mode gets actived. While kill mode is active, we
	 * do not serve incoming RPC requests but we need to stay responsive
	 * to user input. Hence, we wait for signals in the input dispatcher
	 * in this case. An already existing 'Signal' object would lock the
	 * signal receiver and thereby prevent this nested way of signal
	 * handling.
	 */
	Signal_rpc_dispatcher_base *dispatcher = 0;
	unsigned num = 0;

	{
		Signal sig = global_sig_rec().wait_for_signal();
		dispatcher = dynamic_cast<Signal_rpc_dispatcher_base *>(sig.context());
		num        = sig.num();
	}

	if (!dispatcher)
		return;

	if (entrypoint)
		dispatcher->dispatch_at_entrypoint(num);
	else
		dispatcher->dispatch(num);
}

Signal_context_capability Entrypoint::manage(Signal_rpc_dispatcher_base &dispatcher)
{
	return dispatcher.manage(global_sig_rec(), global_rpc_ep());
}


void Server::Entrypoint::dissolve(Signal_rpc_dispatcher_base &dispatcher)
{
	dispatcher.dissolve(global_sig_rec(), global_rpc_ep());
}


Server::Entrypoint::Entrypoint() : _rpc_ep(global_rpc_ep()) { }


void Server::wait_and_dispatch_one_signal() {
	::wait_and_dispatch_one_signal(true); }


namespace Server {
	struct Constructor;
	struct Constructor_component;
}


struct Server::Constructor
{
	GENODE_RPC(Rpc_construct, void, construct);
	GENODE_RPC_INTERFACE(Rpc_construct);
};


struct Server::Constructor_component : Rpc_object<Server::Constructor,
                                                  Server::Constructor_component>
{
	void construct() { Server::construct(global_ep()); }
};


int main(int argc, char **argv)
{
	static Server::Constructor_component constructor;

	static Server::Entrypoint ep;

	/* call Server::construct in the context of the entrypoint */
	Capability<Server::Constructor> constructor_cap = ep.manage(constructor);

	constructor_cap.call<Server::Constructor::Rpc_construct>();

	/* process incoming signals */
	for (;;)
		wait_and_dispatch_one_signal(false);
}
