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
#include <util/xml_node.h>

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

void Dns_domain_name::set_to(Dns_domain_name const &name)
{
	if (name.valid()) {
		name.with_string([&] (String const &string) {
			if (_string.valid()) {
				_string() = string;
			} else {
				_string = *new (_alloc) String { string };
			}
		});
	} else {
		set_invalid();
	}
}


void Dns_domain_name::set_to(Xml_attribute const &name_attr)
{
	name_attr.with_raw_value([&] (char const *base, size_t size) {
		if (size < STRING_CAPACITY) {
			if (_string.valid()) {
				_string() = Cstring { base, size };
			} else {
				_string = *new (_alloc) String { Cstring { base, size } };
			}
		} else {
			set_invalid();
		}
	});
}


void Dns_domain_name::set_to(Dhcp_packet::Domain_name const &name_option)
{
	name_option.with_string([&] (char const *base, size_t size) {
		if (size < STRING_CAPACITY) {
			if (_string.valid()) {
				_string() = Cstring { base, size };
			} else {
				_string = *new (_alloc) String { Cstring { base, size } };
			}
		} else {
			set_invalid();
		}
	});
}


void Dns_domain_name::set_invalid()
{
	if (_string.valid()) {
		_alloc.free(&_string(), sizeof(String));
		_string = { };
	}
}


bool Dns_domain_name::equal_to(Dns_domain_name const &other) const
{
	if (_string.valid()) {
		if (other._string.valid()) {
			return _string() == other._string();
		}
		return false;
	}
	return !other._string.valid();
}


Dns_domain_name::Dns_domain_name(Genode::Allocator &alloc)
:
	_alloc { alloc }
{ }


Dns_domain_name::~Dns_domain_name()
{
	set_invalid();
}
