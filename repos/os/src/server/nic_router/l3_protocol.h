/*
 * \brief  Utilities regarding layer 3 protocols in general
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _L3_PROTOCOL_H_
#define _L3_PROTOCOL_H_

/* Genode includes */
#include <net/ipv4.h>
#include <base/stdint.h>

namespace Genode { class Cstring; }

namespace Net {

	using L3_protocol = Ipv4_packet::Protocol;

	Genode::Cstring const &tcp_name();
	Genode::Cstring const &udp_name();
	Genode::Cstring const &l3_protocol_name(L3_protocol protocol);
}

#endif /* _L3_PROTOCOL_H_ */
