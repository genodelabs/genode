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

/* local includes */
#include <l3_protocol.h>
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


Cstring const &Net::l3_protocol_name(L3_protocol protocol)
{
	switch (protocol) {
	case L3_protocol::TCP: return tcp_name();
	case L3_protocol::UDP: return udp_name();
	default: throw Interface::Bad_transport_protocol(); }
}
