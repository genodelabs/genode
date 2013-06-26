/*
 * \brief  Proxy-ARP for Nic-session
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <cap_session/connection.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic/xml_node.h>
#include <os/config.h>

/* local includes */
#include "component.h"
#include "env.h"


int main(int, char **)
{
	using namespace Genode;

	/* read MAC address prefix from config file */
	try {
		Nic::Mac_address mac;
		Genode::config()->xml_node().attribute("mac").value(&mac);
		Genode::memcpy(&Net::Mac_allocator::mac_addr_base, &mac,
		               sizeof(Net::Mac_allocator::mac_addr_base));
	} catch(...) {}

	try {
		enum { STACK_SIZE = 1024*sizeof(Genode::addr_t) };
		static Cap_connection cap;
		static Rpc_entrypoint ep(&cap, STACK_SIZE, "nic_bridge_ep");
		static Net::Root      nic_root(&ep, env()->heap());

		/* announce NIC service */
		env()->parent()->announce(ep.manage(&nic_root));

		while (true) {
			Signal s = Net::Env::receiver()->wait_for_signal();
			static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
		}
	} catch (Parent::Service_denied) {
		PERR("Could not connect to uplink NIC");
	}

	return 0;
}
