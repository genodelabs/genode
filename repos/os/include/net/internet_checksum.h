/*
 * \brief  Computing the Internet Checksum (conforms to RFC 1071)
 * \author Martin Stein
 * \date   2018-03-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__INTERNET_CHECKSUM_H_
#define _NET__INTERNET_CHECKSUM_H_

/* Genode includes */
#include <net/ipv4.h>
#include <base/stdint.h>

namespace Net {

	Genode::uint16_t internet_checksum(Genode::uint16_t const *addr,
	                                   Genode::size_t          size,
	                                   Genode::addr_t          init_sum = 0);

	Genode::uint16_t internet_checksum_pseudo_ip(Genode::uint16_t const *addr,
	                                             Genode::size_t          size,
	                                             Genode::uint16_t        size_be,
	                                             Ipv4_packet::Protocol   ip_prot,
	                                             Ipv4_address           &ip_src,
	                                             Ipv4_address           &ip_dst);
}

#endif /* _NET__INTERNET_CHECKSUM_H_ */
