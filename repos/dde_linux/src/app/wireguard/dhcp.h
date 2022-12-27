/*
 * \brief  DHCP related local utilities
 * \author Martin Stein
 * \date   2021-02-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DHCP_H_
#define _DHCP_H_

/* os includes */
#include <net/dhcp.h>

namespace Net {

	template <typename T>
	static Ipv4_address dhcp_ipv4_option(Dhcp_packet &dhcp)
	{
		try { return dhcp.option<T>().value(); }
		catch (Dhcp_packet::Option_not_found) { return Ipv4_address { }; }
	}
}

#endif /* _DHCP_H_ */
