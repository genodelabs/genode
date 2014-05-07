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
#include <nic/packet_allocator.h>
#include <util/string.h>

#include <lwip/genode.h>

extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}


extern void create_lwip_plugin();
extern void create_etc_resolv_conf_plugin();


void __attribute__((constructor)) init_nic_dhcp(void)
{
	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	PDBG("init_nic_dhcp()\n");

	bool provide_etc_resolv_conf = true;

	char ip_addr_str[16] = {0};
	char netmask_str[16] = {0};
	char gateway_str[16] = {0};

	genode_int32_t ip_addr = 0;
	genode_int32_t netmask = 0;
	genode_int32_t gateway = 0;
	Genode::Number_of_bytes tx_buf_size(BUF_SIZE);
	Genode::Number_of_bytes rx_buf_size(BUF_SIZE);

	try {
		Genode::Xml_node libc_node = Genode::config()->xml_node().sub_node("libc");

		try {
			if (libc_node.attribute("resolv").has_value("no"))
				provide_etc_resolv_conf = false;
		} catch(...) { }

		try {
			libc_node.attribute("ip_addr").value(ip_addr_str, sizeof(ip_addr_str));
		} catch(...) { }

		try {
			libc_node.attribute("netmask").value(netmask_str, sizeof(netmask_str));
		} catch(...) { }

		try {
			libc_node.attribute("gateway").value(gateway_str, sizeof(gateway_str));
		} catch(...) { }

		try {
			libc_node.attribute("tx_buf_size").value(&tx_buf_size);
		} catch(...) { }

		try {
			libc_node.attribute("rx_buf_size").value(&rx_buf_size);
		} catch(...) { }

		/* either none or all 3 interface attributes must exist */
		if ((strlen(ip_addr_str) != 0) ||
		    (strlen(netmask_str) != 0) ||
		    (strlen(gateway_str) != 0)) {
			if (strlen(ip_addr_str) == 0) {
				PERR("Missing \"ip_addr\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (strlen(netmask_str) == 0) {
				PERR("Missing \"netmask\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (strlen(gateway_str) == 0) {
				PERR("Missing \"gateway\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			}
		} else
			throw -1;

		PDBG("static network interface: ip_addr=%s netmask=%s gateway=%s ",
		     ip_addr_str, netmask_str, gateway_str
		);

		genode_uint32_t ip, nm, gw;
		
		ip = inet_addr(ip_addr_str);
		nm = inet_addr(netmask_str);
		gw = inet_addr(gateway_str);

		if (ip == INADDR_NONE || nm == INADDR_NONE || gw == INADDR_NONE) {
			PERR("Invalid network interface config.");
			throw -1;
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
		lwip_nic_init(ip_addr, netmask, gateway,
		              (Genode::size_t)tx_buf_size, (Genode::size_t)rx_buf_size);
	} catch (Genode::Parent::Service_denied) {
		/* ignore for now */
	}

	if (provide_etc_resolv_conf)
		create_etc_resolv_conf_plugin();
}
