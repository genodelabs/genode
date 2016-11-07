/*
 * \brief  Lxip plugin creation
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2013-09-04
 *
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/snprintf.h>
#include <os/config.h>
#include <util/string.h>

extern void create_lxip_plugin(char const *address_config);

void __attribute__((constructor)) init_libc_lxip(void)
{
	char ip_addr_str[16] = {0};
	char netmask_str[16] = {0};
	char gateway_str[16] = {0};
	char address_buf[128];
	char const *address_config;

	try {
		Genode::Xml_node libc_node = Genode::config()->xml_node().sub_node("libc");

		try {
			libc_node.attribute("ip_addr").value(ip_addr_str, sizeof(ip_addr_str));
		} catch(...) { }

		try {
			libc_node.attribute("netmask").value(netmask_str, sizeof(netmask_str));
		} catch(...) { }

		try {
			libc_node.attribute("gateway").value(gateway_str, sizeof(gateway_str));
		} catch(...) { }

		/* either none or all 3 interface attributes must exist */
		if ((Genode::strlen(ip_addr_str) != 0) ||
		    (Genode::strlen(netmask_str) != 0) ||
		    (Genode::strlen(gateway_str) != 0)) {
			if (Genode::strlen(ip_addr_str) == 0) {
				Genode::error("missing \"ip_addr\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (Genode::strlen(netmask_str) == 0) {
				Genode::error("missing \"netmask\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (Genode::strlen(gateway_str) == 0) {
				Genode::error("missing \"gateway\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			}
		} else
			throw -1;

		Genode::log("static network interface: ",
		            "ip_addr=", Genode::Cstring(ip_addr_str), " "
		            "netmask=", Genode::Cstring(netmask_str), " "
		            "gateway=", Genode::Cstring(gateway_str));

		Genode::snprintf(address_buf, sizeof(address_buf), "%s::%s:%s:::off",
		                 ip_addr_str, gateway_str, netmask_str);
		address_config = address_buf;
	}
	catch (...) {
		Genode::log("Using DHCP for interface configuration.");
		address_config = "dhcp";
	}

	Genode::log("init_libc_lxip() address config=", address_config);

	create_lxip_plugin(address_config);
}
