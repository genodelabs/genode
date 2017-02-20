/*
 * \brief  Provide protocol names as Genode Cstring objects
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <protocol_name.h>
#include <interface.h>

/* Genode includes */
#include <util/string.h>
#include <net/tcp.h>
#include <net/udp.h>

using namespace Net;
using namespace Genode;

static Cstring const _udp_name("UDP");
static Cstring const _tcp_name("TCP");

Cstring const &Net::udp_name() { return _udp_name; }
Cstring const &Net::tcp_name() { return _tcp_name; }


Cstring const &Net::protocol_name(uint8_t protocol)
{
	switch (protocol) {
	case Tcp_packet::IP_ID: return tcp_name();
	case Udp_packet::IP_ID: return udp_name();
	default: throw Interface::Bad_transport_protocol(); }
}
