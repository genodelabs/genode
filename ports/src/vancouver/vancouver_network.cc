/*
 * \brief  Network receive handler per MAC address
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 * 
 * This file is part of the Genode OS framework and being contributed under
 * the terms and conditions of the GNU General Public License version 2.   
 */

#include <vancouver_network.h>

extern const void * _forward_pkt;

Vancouver_network::Vancouver_network(Motherboard &mb, Nic::Session * nic)
: _mb(mb), _nic(nic)
{
	start();
}

void Vancouver_network::entry()
{
	Logging::printf("Hello, this is the network receiver.\n");

	while (true) {
		Packet_descriptor rx_packet = _nic->rx()->get_packet();

		/* Send it to the network bus */
		char * rx_content = _nic->rx()->packet_content(rx_packet);
		_forward_pkt = rx_content;
		MessageNetwork msg((unsigned char *)rx_content, rx_packet.size(), 0);
		_mb.bus_network.send(msg);
		_forward_pkt = 0;

		/* acknowledge received packet */
		_nic->rx()->acknowledge_packet(rx_packet);
	}
}
