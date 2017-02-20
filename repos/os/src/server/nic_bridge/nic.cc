/*
 * \brief  NIC handler
 * \author Stefan Kalkowski
 * \date   2013-05-24
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/dhcp.h>

#include <component.h>

using namespace Net;


bool Net::Nic::handle_arp(Ethernet_frame *eth, Genode::size_t size) {
	Arp_packet *arp = new (eth->data<void>())
		Arp_packet(size - sizeof(Ethernet_frame));

	/* ignore broken packets */
	if (!arp->ethernet_ipv4())
		return true;

	/* look whether the IP address is one of our client's */
	Ipv4_address_node *node = vlan().ip_tree.first();
	if (node)
		node = node->find_by_address(arp->dst_ip());
	if (node) {
		if (arp->opcode() == Arp_packet::REQUEST) {
			/*
			 * The ARP packet gets re-written, we interchange source
			 * and destination MAC and IP addresses, and set the opcode
			 * to reply, and then push the packet back to the NIC driver.
			 */
			Ipv4_address old_src_ip = arp->src_ip();
			arp->opcode(Arp_packet::REPLY);
			arp->dst_mac(arp->src_mac());
			arp->src_mac(mac());
			arp->src_ip(arp->dst_ip());
			arp->dst_ip(old_src_ip);
			eth->dst(arp->dst_mac());

			/* set our MAC as sender */
			eth->src(mac());
			send(eth, size);
		} else {
			/* overwrite destination MAC */
			arp->dst_mac(node->component().mac_address().addr);
			eth->dst(node->component().mac_address().addr);
			node->component().send(eth, size);
		}
		return false;
	}
	return true;
}


bool Net::Nic::handle_ip(Ethernet_frame *eth, Genode::size_t size) {
	Ipv4_packet *ip = new (eth->data<void>())
		Ipv4_packet(size - sizeof(Ethernet_frame));

	/* is it an UDP packet ? */
	if (ip->protocol() == Udp_packet::IP_ID)
	{
		Udp_packet *udp = new (ip->data<void>())
			Udp_packet(size - sizeof(Ipv4_packet));

		/* is it a DHCP packet ? */
		if (Dhcp_packet::is_dhcp(udp)) {
			Dhcp_packet *dhcp = new (udp->data<void>())
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
							vlan().mac_tree.first();
						if (node)
							node = node->find_by_address(dhcp->client_mac());
						if (node)
							node->component().set_ipv4_address(dhcp->yiaddr());
					}
				}
			}
		}
	}

	/* is it an unicast message to one of our clients ? */
	if (eth->dst() == mac()) {
		Ipv4_address_node *node = vlan().ip_tree.first();
		if (node) {
			node = node->find_by_address(ip->dst());
			if (node) {
				/* overwrite destination MAC */
				eth->dst(node->component().mac_address().addr);

				/* deliver the packet to the client */
				node->component().send(eth, size);
				return false;
			}
		}
	}
	return true;
}


Net::Nic::Nic(Genode::Env &env, Genode::Heap &heap, Net::Vlan &vlan)
: Packet_handler(env.ep(), vlan),
  _tx_block_alloc(&heap),
  _nic(env, &_tx_block_alloc, BUF_SIZE, BUF_SIZE),
  _mac(_nic.mac_address().addr)
{
	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
	_nic.link_state_sigh(_client_link_state);
}
