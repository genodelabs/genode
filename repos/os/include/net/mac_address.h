/*
 * \brief  Media access control (MAC) address
 * \author Martin Stein
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NET__MAC_ADDRESS_H_
#define _NET__MAC_ADDRESS_H_

/* OS includes */
#include <net/netaddress.h>

namespace Net
{
	using Mac_address = Net::Network_address<6, ':', true>;

	Mac_address mac_from_string(const char * mac);
}

#endif /* _NET__MAC_ADDRESS_H_ */
