/*
 * \brief  IPv4 peer configuration
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* dde_linux wireguard includes */
#include <ipv4_config.h>

using namespace Genode;
using namespace Net;
using namespace Wireguard;


Ipv4_config::Ipv4_config()
:
	_interface { }
{ }


Ipv4_config::Ipv4_config(Xml_node const &config_node)
:
	_interface { config_node.attribute_value("interface",  Ipv4_address_prefix()) }
{ }


Ipv4_config::Ipv4_config(Dhcp_packet &dhcp_ack)
:
	_interface  { dhcp_ack.yiaddr(),
	              dhcp_ipv4_option<Dhcp_packet::Subnet_mask>(dhcp_ack) }
{ }
