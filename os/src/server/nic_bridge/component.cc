/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/udp.h>

#include <util/token.h>
#include <util/string.h>

#include "component.h"
#include "vlan.h"

using namespace Net;

const int Session_component::verbose = 1;

void Session_component::Tx_handler::acknowledge_last_one()
{
	if (!_tx_packet.valid())
		return;

	if (!_component->tx_sink()->ready_to_ack())
		PDBG("need to wait until ready-for-ack");
	_component->tx_sink()->acknowledge_packet(_tx_packet);
}


void Session_component::Tx_handler::next_packet(void** src, Genode::size_t *size)
{
	while (true) {
		/* block for a new packet */
		_tx_packet = _component->tx_sink()->get_packet();
		if (!_tx_packet.valid()) {
			PWRN("received invalid packet");
			continue;
		}
		*src  = _component->tx_sink()->packet_content(_tx_packet);
		*size = _tx_packet.size();
		return;
	}
}


bool Session_component::Tx_handler::handle_arp(Ethernet_frame *eth, Genode::size_t size)
{
	Arp_packet *arp =
		new (eth->data()) Arp_packet(size - sizeof(Ethernet_frame));
	if (arp->ethernet_ipv4() &&
		arp->opcode() == Arp_packet::REQUEST) {
		Ipv4_address_node *node = Vlan::vlan()->ip_tree()->first();
		if (node)
			node = node->find_by_address(arp->dst_ip());
		if (!node) {
			arp->src_mac(_mac);
		}
	}
	return true;
}


bool Session_component::Tx_handler::handle_ip(Ethernet_frame *eth, Genode::size_t size)
{
	Ipv4_packet *ip =
		new (eth->data()) Ipv4_packet(size - sizeof(Ethernet_frame));

	if (ip->protocol() == Udp_packet::IP_ID)
	{
		Udp_packet *udp = new (ip->data())
			Udp_packet(size - sizeof(Ipv4_packet));
		if (Dhcp_packet::is_dhcp(udp)) {
			Dhcp_packet *dhcp = new (udp->data())
				Dhcp_packet(size - sizeof(Ipv4_packet) - sizeof(Udp_packet));
			if (dhcp->op() == Dhcp_packet::REQUEST) {
				dhcp->broadcast(true);
				udp->calc_checksum(ip->src(), ip->dst());
			}
		}
	}
	return true;
}


void Session_component::Tx_handler::finalize_packet(Ethernet_frame *eth,
                                                    Genode::size_t size)
{
	Mac_address_node *node = Vlan::vlan()->mac_tree()->first();
	if (node)
		node = node->find_by_address(eth->dst());
	if (node)
		node->receive_packet((void*) eth, size);
	else
		send_to_nic(eth, size);
}


void Session_component::_free_ipv4_node()
{
	if (_ipv4_node) {
		Vlan::vlan()->ip_tree()->remove(_ipv4_node);
		destroy(this->guarded_allocator(), _ipv4_node);
	}
}


struct Scanner_policy_number
{
	static bool identifier_char(char c, unsigned  i )
	{
		return Genode::is_digit(c) && c !='.';
	}
};


typedef ::Genode::Token<Scanner_policy_number> Token;


Ipv4_packet::Ipv4_address Session_component::ip_from_string(const char *ip)
{
	Ipv4_packet::Ipv4_address ip_addr;

	Token       t(ip);
	char        tmpstr[4];
	int         cnt = 0;
	unsigned char ipb[4] = {0};

	while(t) {

		if (t.type() == Token::WHITESPACE || t[0] == '.') {
			t = t.next();
			continue;
		}
		t.string(tmpstr, sizeof(tmpstr));

		unsigned long  tmpc = 0;
		Genode::ascii_to(tmpstr, &tmpc, 10);

		ipb[cnt] = tmpc & 0xFF;

		t = t.next();

		if (cnt == 4)
			break;
		cnt++;
	}

	if (cnt == 4) {
		ip_addr.addr[0] = ipb[0];
		ip_addr.addr[1] = ipb[1];
		ip_addr.addr[2] = ipb[2];
		ip_addr.addr[3] = ipb[3];
	}
	
	return ip_addr;
}



Session_component::Session_component(Genode::Allocator          *allocator,
                                     Genode::size_t              amount,
                                     Genode::size_t              tx_buf_size,
                                     Genode::size_t              rx_buf_size,
                                     Ethernet_frame::Mac_address vmac,
                                     Nic::Connection            *session,
                                     Genode::Rpc_entrypoint     &ep,
                                     char                       *ip_addr)
: Guarded_range_allocator(allocator, amount),
  Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),
  Session_rpc_object(Tx_rx_communication_buffers::tx_ds(),
                     Tx_rx_communication_buffers::rx_ds(),
                     this->range_allocator(), ep),
  _tx_handler(session, this),
  _mac_node(vmac, this),
  _ipv4_node(0)
{
	Vlan::vlan()->mac_tree()->insert(&_mac_node);
	Vlan::vlan()->mac_list()->insert(&_mac_node);

	/* start handler */
	_tx_handler.start();
	_tx_handler.wait_for_startup();

	/* static ip parsing */
	if (ip_addr != 0 && Genode::strlen(ip_addr)) {
		
		Ipv4_packet::Ipv4_address ip = ip_from_string(ip_addr);
		
		if (ip == Ipv4_packet::Ipv4_address()) {
			PDBG("Empty or error ip address. Skipped.");
		} else {
			
			set_ipv4_address(ip);

			if (verbose)
				PDBG("\nmac=%02x.%02x.%02x.%02x.%02x.%02x ip=%d.%d.%d.%d",
					vmac.addr[0], vmac.addr[1], vmac.addr[2], vmac.addr[3], vmac.addr[4], vmac.addr[5],
					ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]
				);
		}
	}
}


Session_component::~Session_component() {
	Vlan::vlan()->mac_tree()->remove(&_mac_node);
	Vlan::vlan()->mac_list()->remove(&_mac_node);
	_free_ipv4_node();
}


void Session_component::set_ipv4_address(Ipv4_packet::Ipv4_address ip_addr)
{
	_free_ipv4_node();
	_ipv4_node = new (this->guarded_allocator())
		Ipv4_address_node(ip_addr, this);
	Vlan::vlan()->ip_tree()->insert(_ipv4_node);
}
