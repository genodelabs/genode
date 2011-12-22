/*
 * \brief  Proxy-ARP for Nic-session
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <nic_session/connection.h>

#include "packet_handler.h"
#include "component.h"


int main(int, char **)
{
	using namespace Genode;

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nic_bridge_ep");

	static Genode::Allocator_avl tx_block_alloc(env()->heap());

	Root_capability nic_root_cap;
	try {
		static Nic::Connection nic(&tx_block_alloc);
		static Net::Rx_handler rx_handler(&nic);
		static Net::Root       nic_root(&ep, env()->heap(), &nic);

		/* start receiver thread handling packets from the NIC driver */
		rx_handler.start();
		rx_handler.wait_for_startup();

		/* announce NIC service */
		env()->parent()->announce(ep.manage(&nic_root));

	} catch (Parent::Service_denied) {
		PERR("Could not connect to uplink NIC");
	}

	sleep_forever();
	return 0;
}
