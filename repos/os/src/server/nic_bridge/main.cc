/*
 * \brief  Proxy-ARP for Nic-session
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <nic/xml_node.h> /* ugly template dependency forces us
                             to include this before xml_node.h */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/log.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include <component.h>


struct Main
{
	Genode::Env                    &env;
	Genode::Entrypoint             &ep     { env.ep() };
	Genode::Heap                    heap   { env.ram(), env.rm() };
	Genode::Attached_rom_dataspace  config { env, "config" };
	Net::Vlan                       vlan;
	Net::Nic                        nic    { env, heap, vlan };
	Net::Root                       root   { env, nic, heap, config.xml() };

	void handle_config()
	{
		/* read MAC address prefix from config file */
		try {
			Nic::Mac_address mac;
			config.xml().attribute("mac").value(&mac);
			Genode::memcpy(&Net::Mac_allocator::mac_addr_base, &mac,
			               sizeof(Net::Mac_allocator::mac_addr_base));
		} catch(...) {}
	}

	Main(Genode::Env &e) : env(e)
	{
		try {
			/* read configuration file */
			handle_config();

			/* show MAC address to use */
			Net::Mac_address mac(nic.mac());
			Genode::log("--- NIC bridge started (mac=", mac, ") ---");

			/* announce at parent */
			env.parent().announce(ep.manage(root));
		} catch (Genode::Service_denied) {
			Genode::error("Could not connect to uplink NIC");
		}
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main nic_bridge(env);
}
