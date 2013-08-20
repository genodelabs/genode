/*
 * \brief  Network receive handler per MAC address
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* local includes */
#include <network.h>

extern const void * _forward_pkt;


Vancouver_network::Vancouver_network(Synced_motherboard &mb, Nic::Session *nic)
: Thread("vmm_network"), _motherboard(mb), _nic(nic)
{
	start();
}


void Vancouver_network::entry()
{
	while (true) {
		Packet_descriptor rx_packet = _nic->rx()->get_packet();

		/* send it to the network bus */
		char * rx_content = _nic->rx()->packet_content(rx_packet);
		_forward_pkt = rx_content;
		MessageNetwork msg((unsigned char *)rx_content, rx_packet.size(), 0);
		_motherboard()->bus_network.send(msg);
		_forward_pkt = 0;

		/* acknowledge received packet */
		_nic->rx()->acknowledge_packet(rx_packet);
	}
}
