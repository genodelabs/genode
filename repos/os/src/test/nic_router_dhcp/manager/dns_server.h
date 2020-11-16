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

#ifndef _DNS_SERVER_H_
#define _DNS_SERVER_H_

/* local includes */
#include <list.h>

/* Genode includes */
#include <net/ipv4.h>

namespace Local { class Dns_server; }

class Local::Dns_server : private Genode::Noncopyable,
                          public  Local::List<Dns_server>::Element
{
	private:

		Net::Ipv4_address const _ip;

	public:

		struct Invalid : Genode::Exception { };

		Dns_server(Net::Ipv4_address const &ip);

		bool equal_to(Dns_server const &server) const;


		/***************
		 ** Accessors **
		 ***************/

		Net::Ipv4_address const &ip() const { return _ip; }
};



#endif /* _DHCP_SERVER_H_ */
