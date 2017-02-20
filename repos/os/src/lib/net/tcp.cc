/*
 * \brief  Transmission Control Protocol
 * \author Martin Stein
 * \date   2016-06-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/tcp.h>
#include <base/output.h>


void Net::Tcp_packet::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mTCP\033[0m ", src_port(),
	              " > ", dst_port(), " flags '");

	if (fin()) { Genode::print(output, "f"); }
	if (syn()) { Genode::print(output, "s"); }
	if (rst()) { Genode::print(output, "r"); }
	if (psh()) { Genode::print(output, "p"); }
	if (ack()) { Genode::print(output, "a"); }
	if (urg()) { Genode::print(output, "u"); }
	Genode::print(output, "' ");
}
