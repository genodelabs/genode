/*
 * \brief  Thread implementations handling network packets.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/lock.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>

#include "component.h"
#include "packet_handler.h"
#include "vlan.h"

using namespace Net;


static Genode::Lock _nic_lock;


void Packet_handler::broadcast_to_clients(Ethernet_frame *eth, Genode::size_t size)
{
	/* check whether it's really a broadcast packet */
	if (eth->dst() == Ethernet_frame::BROADCAST) {
		/* iterate through the list of clients */
		Mac_address_node *node =
			Vlan::vlan()->mac_list()->first();
		while (node) {
			/* deliver packet */
			node->receive_packet((void*) eth, size);
			node = node->next();
		}
	}
}


void Packet_handler::send_to_nic(Ethernet_frame *eth, Genode::size_t  size)
{
	Genode::Lock::Guard lock_guard(_nic_lock);

	while (true) {
		/* check for acknowledgements */
		while (_session->tx()->ack_avail()) {
			Packet_descriptor p =
				_session->tx()->get_acked_packet();
			_session->tx()->release_packet(p);
		}

		try {
			/* set our MAC as sender */
			eth->src(_mac);

			/* allocate packet to NIC driver */
			Packet_descriptor tx_packet = _session->tx()->alloc_packet(size);
			char *tx_content = _session->tx()->packet_content(tx_packet);

			/* copy and submit packet */
			Genode::memcpy((void*)tx_content, (void*)eth, size);
			_session->tx()->submit_packet(tx_packet);
			return;
		} catch(Nic::Session::Tx::Source::Packet_alloc_failed) { }
	}
}


void Packet_handler::entry()
{
	void*          src;
	Genode::size_t eth_sz;

	/* signal preparedness */
	_startup_sem.up();

	/* loop for new packets */
	while (true) {
		try {
			acknowledge_last_one();
			next_packet(&src, &eth_sz);

			/* parse ethernet frame header */
			Ethernet_frame *eth = new (src) Ethernet_frame(eth_sz);
			switch (eth->type()) {
			case Ethernet_frame::ARP:
				{
					if (!handle_arp(eth, eth_sz))
						continue;
					break;
				}
			case Ethernet_frame::IPV4:
				{
					if(!handle_ip(eth, eth_sz))
						continue;
					break;
				}
			default:
				;
			}

			/* broadcast packet ? */
			broadcast_to_clients(eth, eth_sz);

			finalize_packet(eth, eth_sz);
		} catch(Arp_packet::No_arp_packet) {
			PWRN("Invalid ARP packet!");
		} catch(Ethernet_frame::No_ethernet_frame) {
			PWRN("Invalid ethernet frame");
		} catch(Dhcp_packet::No_dhcp_packet) {
			PWRN("Invalid IPv4 packet!");
		} catch(Ipv4_packet::No_ip_packet) {
			PWRN("Invalid IPv4 packet!");
		} catch(Udp_packet::No_udp_packet) {
			PWRN("Invalid UDP packet!");
		}
	}
}


void Rx_handler::acknowledge_last_one() {
	/* acknowledge packet to NIC driver */
	if(_rx_packet.valid())
		_session->rx()->acknowledge_packet(_rx_packet);
}


void Rx_handler::next_packet(void** src, Genode::size_t *size) {
	/* get next packet from NIC driver */
	_rx_packet = _session->rx()->get_packet();
	*src       = _session->rx()->packet_content(_rx_packet);
	*size      = _rx_packet.size();
}


bool Rx_handler::handle_arp(Ethernet_frame *eth, Genode::size_t size) {
	Arp_packet *arp = new (eth->data())
		Arp_packet(size - sizeof(Ethernet_frame));

	/* ignore broken packets */
	if (!arp->ethernet_ipv4())
		return true;

	/* look whether the IP address is one of our client's */
	Ipv4_address_node *node = Vlan::vlan()->ip_tree()->first();
	if (node)
		node = node->find_by_address(arp->dst_ip());
	if (node) {
		if (arp->opcode() == Arp_packet::REQUEST) {
			/*
			 * The ARP packet gets re-written, we interchange source
			 * and destination MAC and IP addresses, and set the opcode
			 * to reply, and then push the packet back to the NIC driver.
			 */
			Ipv4_packet::Ipv4_address old_src_ip = arp->src_ip();
			arp->opcode(Arp_packet::REPLY);
			arp->dst_mac(arp->src_mac());
			arp->src_mac(_mac);
			arp->src_ip(arp->dst_ip());
			arp->dst_ip(old_src_ip);
			eth->dst(arp->dst_mac());
			send_to_nic(eth, size);
		} else {
			/* overwrite destination MAC */
			arp->dst_mac(node->component()->mac_address().addr);
			eth->dst(node->component()->mac_address().addr);
			node->receive_packet((void*) eth, size);
		}
		return false;
	}
	return true;
}


bool Rx_handler::handle_ip(Ethernet_frame *eth, Genode::size_t size) {
	Ipv4_packet *ip = new (eth->data())
		Ipv4_packet(size - sizeof(Ethernet_frame));

	/* is it an UDP packet ? */
	if (ip->protocol() == Udp_packet::IP_ID)
	{
		Udp_packet *udp = new (ip->data())
			Udp_packet(size - sizeof(Ipv4_packet));

		/* is it a DHCP packet ? */
		if (Dhcp_packet::is_dhcp(udp)) {
			Dhcp_packet *dhcp = new (udp->data())
				Dhcp_packet(size - sizeof(Ipv4_packet) - sizeof(Udp_packet));

			/* check for DHCP ACKs containing new client ips */
			if (dhcp->op() == Dhcp_packet::REPLY) {
				Dhcp_packet::Option *ext = dhcp->option(Dhcp_packet::MSG_TYPE);
				if (ext) {
					/*
					 * extract the IP address and set it in the
					 * client's session-component
					 */
					Genode::uint8_t *msg_type =	(Genode::uint8_t*) ext->value();
					if (*msg_type == Dhcp_packet::DHCP_ACK) {
						Mac_address_node *node =
							Vlan::vlan()->mac_tree()->first();
						if (node)
							node = node->find_by_address(dhcp->client_mac());
						if (node)
							node->component()->set_ipv4_address(dhcp->yiaddr());
					}
				}
			}
		}
	}

	/* is it an unicast message to one of our clients ? */
	if (eth->dst() == _mac) {
		Ipv4_address_node *node = Vlan::vlan()->ip_tree()->first();
		if (node) {
			node = node->find_by_address(ip->dst());
			if (node) {
				/* overwrite destination MAC */
				eth->dst(node->component()->mac_address().addr);

				/* deliver the packet to the client */
				node->receive_packet((void*) eth, size);
				return false;
			}
		}
	}
	return true;
}
