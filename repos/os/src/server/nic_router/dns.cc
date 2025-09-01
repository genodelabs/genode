/*
 * \brief  Utilities for handling DNS configurations
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
#include <dns.h>
#include <domain.h>
#include <configuration.h>

/* Genode includes */
#include <base/node.h>

using namespace Net;
using namespace Genode;


/****************
 ** Dns_server **
 ****************/

Dns_server::Dns_server(Ipv4_address const &ip)
:
	_ip { ip }
{ }


bool Dns_server::equal_to(Dns_server const &server) const
{
	return _ip == server._ip;
}


/*********************
 ** Dns_domain_name **
 *********************/

void Dns_domain_name::set_to(String const &name)
{
	if (_string_valid(name))
		_string = name;
	else
		_string = { };
}


void Dns_domain_name::set_to(Dhcp_packet::Domain_name const &name_option)
{
	name_option.with_string([&] (char const *base, size_t size) {
		if (size < String::capacity() - 1)
			_string = { Cstring { base, size } };
		else
			_string = { };
	});
}


bool Dns_domain_name::equal_to(Dns_domain_name const &other) const
{
	return other._string == _string;
}

