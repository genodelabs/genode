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
	Genode::Entrypoint             &ep        { env.ep() };
	Genode::Heap                    heap      { env.ram(), env.rm() };
	Genode::Attached_rom_dataspace  config    { env, "config" };
	Net::Vlan                       vlan      { };
	Genode::Session_label     const nic_label { "uplink" };
	bool                      const verbose   { config.xml().attribute_value("verbose", false) };
	Net::Nic                        nic       { env, heap, vlan, verbose,
	                                            nic_label };
	Net::Root                       root      { env, nic, heap, verbose,
	                                            config.xml() };

	Main(Genode::Env &e) : env(e)
	{
		try {
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


void Component::construct(Genode::Env &env) { static Main nic_bridge(env); }
