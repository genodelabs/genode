/*
 * \brief  Packet handler handling network packets.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>

#include <component.h>
#include <packet_handler.h>

using namespace Net;

void Packet_handler::_ready_to_submit()
{
	/* as long as packets are available, and we can ack them */
	while (sink()->packet_avail()) {
		_packet = sink()->get_packet();
		if (!_packet.size()) continue;
		handle_ethernet(sink()->packet_content(_packet), _packet.size());

		if (!sink()->ready_to_ack()) {
			Genode::warning("ack state FULL");
			return;
		}

		sink()->acknowledge_packet(_packet);
	}
}


void Packet_handler::_ready_to_ack()
{
	/* check for acknowledgements */
	while (source()->ack_avail())
		source()->release_packet(source()->get_acked_packet());
}


void Packet_handler::_link_state()
{
	Mac_address_node *node = _vlan.mac_list.first();
	while (node) {
		node->component().link_state_changed();
		node = node->next();
	}
}


void Packet_handler::broadcast_to_clients(Ethernet_frame *eth, Genode::size_t size)
{
	/* check whether it's really a broadcast packet */
	if (eth->dst() == Ethernet_frame::broadcast()) {
		/* iterate through the list of clients */
		Mac_address_node *node =
			_vlan.mac_list.first();
		while (node) {
			/* deliver packet */
			node->component().send(eth, size);
			node = node->next();
		}
	}
}


void Packet_handler::handle_ethernet(void* src, Genode::size_t size)
{
	try {
		/* parse ethernet frame header */
		Size_guard size_guard(size);
		Ethernet_frame &eth = Ethernet_frame::cast_from(src, size_guard);
		switch (eth.type()) {
		case Ethernet_frame::Type::ARP:
			if (!handle_arp(eth, size_guard)) return;
			break;
		case Ethernet_frame::Type::IPV4:
			if (!handle_ip(eth, size_guard)) return;
			break;
		default:
			;
		}
		broadcast_to_clients(&eth, size);
		finalize_packet(&eth, size);
	} catch(Size_guard::Exceeded) {
		Genode::warning("Packet size guard exceeded!");
	}
}


void Packet_handler::send(Ethernet_frame *eth, Genode::size_t size)
{
	try {
		/* copy and submit packet */
		Packet_descriptor packet  = source()->alloc_packet(size);
		char             *content = source()->packet_content(packet);
		Genode::memcpy((void*)content, (void*)eth, size);
		source()->submit_packet(packet);
	} catch(Packet_stream_source< ::Nic::Session::Policy>::Packet_alloc_failed) {
		Genode::warning("Packet dropped");
	}
}


Packet_handler::Packet_handler(Genode::Entrypoint &ep, Vlan &vlan)
: _vlan(vlan),
  _sink_ack(ep, *this, &Packet_handler::_ack_avail),
  _sink_submit(ep, *this, &Packet_handler::_ready_to_submit),
  _source_ack(ep, *this, &Packet_handler::_ready_to_ack),
  _source_submit(ep, *this, &Packet_handler::_packet_avail),
  _client_link_state(ep, *this, &Packet_handler::_link_state)
{ }
