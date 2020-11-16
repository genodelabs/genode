/*
 * \brief  DNS server entry of a DHCP server or IPv4 config
 * \author Martin Stein
 * \date   2020-11-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <dns_server.h>

using namespace Net;
using namespace Genode;


Local::Dns_server::Dns_server(Ipv4_address const &ip)
:
	_ip { ip }
{
	if (!_ip.valid()) {
		throw Invalid { };
	}
}


bool Local::Dns_server::equal_to(Dns_server const &server) const
{
	return _ip == server._ip;
}
