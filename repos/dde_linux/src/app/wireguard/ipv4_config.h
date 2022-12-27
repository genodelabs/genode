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
#include <dhcp.h>

/* Genode includes */
#include <util/xml_node.h>

namespace Wireguard {

	class Ipv4_config;
}

class Wireguard::Ipv4_config
{
	private:

		Net::Ipv4_address_prefix const _interface;
		bool                     const _interface_valid { _interface.valid() };
		bool                     const _valid           { _interface_valid };

	public:

		Ipv4_config(Net::Dhcp_packet &dhcp_ack);

		Ipv4_config(Genode::Xml_node const &config_node);

		Ipv4_config();

		bool operator != (Ipv4_config const &other) const
		{
			return _interface != other._interface;
		}


		/***************
		 ** Accessors **
		 ***************/

		bool                            valid()     const { return _valid; }
		Net::Ipv4_address_prefix const &interface() const { return _interface; }
};

#endif /* _IPV4_CONFIG_H_ */
