/*
 * \brief  Network receive handler per MAC address
 * \author Markus Partheymueller
 * \author Alexander Boettcher
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* local includes */
#include "network.h"


Seoul::Network::Network(Genode::Env &env, Genode::Heap &heap,
                        Synced_motherboard &mb)
:
	_motherboard(mb), _tx_block_alloc(&heap),
	_nic(env, &_tx_block_alloc, BUF_SIZE, BUF_SIZE),
	_packet_avail(env.ep(), *this, &Network::_handle_packets)
{
	_nic.rx_channel()->sigh_packet_avail(_packet_avail);
}


void Seoul::Network::_handle_packets()
{
	while (_nic.rx()->packet_avail()) {

		Nic::Packet_descriptor rx_packet = _nic.rx()->get_packet();

		/* send it to the network bus */
		char * rx_content = _nic.rx()->packet_content(rx_packet);
		_forward_pkt = rx_content;
		MessageNetwork msg((unsigned char *)rx_content, rx_packet.size(), 0);
		_motherboard()->bus_network.send(msg);
		_forward_pkt = 0;

		/* acknowledge received packet */
		_nic.rx()->acknowledge_packet(rx_packet);
	}
}


bool Seoul::Network::transmit(void const * const packet, Genode::size_t len)
{
	if (packet == _forward_pkt)
		/* don't end in an endless forwarding loop */
		return false;

	/* allocate transmit packet */
	Nic::Packet_descriptor tx_packet;
	try {
		tx_packet = _nic.tx()->alloc_packet(len);
	} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
		Logging::printf("error: tx packet alloc failed\n");
		return false;
	}

	/* fill packet with content */
	char * const tx_content = _nic.tx()->packet_content(tx_packet);
	_forward_pkt = tx_content;
	memcpy(tx_content, packet, len);

	_nic.tx()->submit_packet(tx_packet);

	/* wait for acknowledgement */
	Nic::Packet_descriptor ack_tx_packet = _nic.tx()->get_acked_packet();

	if (ack_tx_packet.size() != tx_packet.size() ||
		ack_tx_packet.offset() != tx_packet.offset())
		Logging::printf("error: unexpected acked packet\n");

	/* release sent packet to free the space in the tx communication buffer */
	_nic.tx()->release_packet(tx_packet);

	return true;
}
