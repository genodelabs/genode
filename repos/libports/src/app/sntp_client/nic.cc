/*
 * \brief  NIC connection wrapper for a more convenient interface
 * \author Martin Stein
 * \date   2018-04-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <nic.h>

using namespace Net;
using namespace Genode;


void Net::Nic::_ready_to_ack()
{
	while (_source().ack_avail()) {
		_source().release_packet(_source().get_acked_packet()); }
}


void Net::Nic::_ready_to_submit()
{
	while (_sink().packet_avail()) {

		Packet_descriptor const pkt = _sink().get_packet();
		if (!pkt.size()) {
			continue; }

		Size_guard size_guard(pkt.size());
		_handler.handle_eth(Ethernet_frame::cast_from(_sink().packet_content(pkt), size_guard),
		                    size_guard);

		if (!_sink().ready_to_ack()) {
			error("ack state FULL");
			return;
		}
		_sink().acknowledge_packet(pkt);
	}
}
