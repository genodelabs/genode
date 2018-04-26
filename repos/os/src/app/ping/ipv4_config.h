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

#ifndef _IPV4_CONFIG_H_
#define _IPV4_CONFIG_H_

/* local includes */
#include <ipv4_address_prefix.h>

namespace Net { class Ipv4_config; }

struct Net::Ipv4_config
{
	Ipv4_address_prefix const interface       { };
	bool                const interface_valid { interface.valid() };
	Ipv4_address        const gateway         { };
	bool                const gateway_valid   { gateway.valid() };
	Ipv4_address        const dns_server      { };
	bool                const valid           { interface_valid &&
	                                            (!gateway_valid ||
	                                             interface.prefix_matches(gateway)) };

	Ipv4_config(Ipv4_address_prefix interface,
	            Ipv4_address        gateway,
	            Ipv4_address        dns_server);

	Ipv4_config() { }

	bool operator != (Ipv4_config const &other) const
	{
		return interface  != other.interface ||
		       gateway    != other.gateway ||
		       dns_server != other.dns_server;
	}

	void print(Genode::Output &output) const;
};

#endif /* _IPV4_CONFIG_H_ */
