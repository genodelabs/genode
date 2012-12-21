/*
 * \brief  lwip nic interface initialisation using DHCP
 * \author Christian Prochaska
 * \date   2010-04-29
 *
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <parent/parent.h>

#include <os/config.h>
#include <util/string.h>

#include <lwip/genode.h>

extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}


extern void create_lwip_plugin();


void __attribute__((constructor)) init_nic_dhcp(void)
{
	PDBG("init_nic_dhcp()\n");

	char ip_addr_str[16] = {0};
	char netmask_str[16] = {0};
	char gateway_str[16] = {0};

	genode_int32_t ip_addr = 0;
	genode_int32_t netmask = 0;
	genode_int32_t gateway = 0;
	
	try {
		Genode::Xml_node interface_node = Genode::config()->xml_node().sub_node("interface");

		try {
			interface_node.attribute("ip_addr").value(ip_addr_str, sizeof(ip_addr_str));
		}
		catch(Genode::Xml_node::Nonexistent_attribute)
		{
			PERR("Missing \"ip_addr\" attribute. Ignore interface config.");
			throw;
		}

		try {
			interface_node.attribute("netmask").value(netmask_str, sizeof(netmask_str));
		}
		catch(Genode::Xml_node::Nonexistent_attribute)
		{
			PERR("Missing \"netmask\" attribute. Ignore interface config.");
			throw;
		}

		try {
			interface_node.attribute("gateway").value(gateway_str, sizeof(gateway_str));
		}
		catch(Genode::Xml_node::Nonexistent_attribute)
		{
			PERR("Missing \"gateway\" attribute. Ignore interface config.");
			throw;
		}
		
		PDBG("interface: ip_addr=%s netmask=%s gateway=%s ",
		     ip_addr_str, netmask_str, gateway_str
		);

		genode_int32_t ip, nm, gw;
		
		ip = inet_addr(ip_addr_str);
		nm = inet_addr(netmask_str);
		gw = inet_addr(gateway_str);

		if (ip == INADDR_NONE || nm == INADDR_NONE || gw == INADDR_NONE) {
			PERR("Invalid interface config.");
			throw;
		} else {
			ip_addr = ip;
			netmask = nm;
			gateway = gw;
		}
	}
	catch (...) {
		PINF("Using DHCP for interface configuration.");
	}

	/* make sure the libc_lwip plugin has been created */
	create_lwip_plugin();

	try {
		lwip_nic_init(ip_addr, netmask, gateway);
	} catch (Genode::Parent::Service_denied) {
		/* ignore for now */
	}
}
