/*
 * \brief  Media access control (MAC) address
 * \author Martin Stein
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__MAC_ADDRESS_H_
#define _NET__MAC_ADDRESS_H_

/* OS includes */
#include <net/netaddress.h>

namespace Net { using Mac_address = Net::Network_address<6, ':', true>; }

#endif /* _NET__MAC_ADDRESS_H_ */
