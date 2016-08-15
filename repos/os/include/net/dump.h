/*
 * \brief  Dump header info of network packets
 * \author Martin Stein
 * \date   2016-08-15
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NET__DUMP_H_
#define _NET__DUMP_H_

/* Genode includes */
#include <base/printf.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/tcp.h>

namespace Net
{
	void dump_tcp(void * const base, Genode::size_t const size)
	{
		Tcp_packet * tcp = new (base) Tcp_packet(size);
		Genode::printf(
			"\033[32mTCP\033[0m %u > %u flags '",
			tcp->src_port(),
			tcp->dst_port());

		if (tcp->fin()) { Genode::printf("f"); }
		if (tcp->syn()) { Genode::printf("s"); }
		if (tcp->rst()) { Genode::printf("r"); }
		if (tcp->psh()) { Genode::printf("p"); }
		if (tcp->ack()) { Genode::printf("a"); }
		if (tcp->urg()) { Genode::printf("u"); }
		Genode::printf("' ");
	}

	void dump_dhcp(void * const base, Genode::size_t const size)
	{
		Dhcp_packet * dhcp = new (base) Dhcp_packet(size);
		Genode::printf(
			"\033[32mDHCP\033[0m %x:%x:%x:%x:%x:%x > %u.%u.%u.%u cmd %u ",
			dhcp->client_mac().addr[0],
			dhcp->client_mac().addr[1],
			dhcp->client_mac().addr[2],
			dhcp->client_mac().addr[3],
			dhcp->client_mac().addr[4],
			dhcp->client_mac().addr[5],
			dhcp->siaddr().addr[0],
			dhcp->siaddr().addr[1],
			dhcp->siaddr().addr[2],
			dhcp->siaddr().addr[3],
			dhcp->op());
	}

	void dump_udp(void * const base, Genode::size_t const size)
	{
		Udp_packet * udp = new (base) Udp_packet(size);
		Genode::printf(
			"\033[32mUDP\033[0m %u > %u ",
			udp->src_port(),
			udp->dst_port());

		Genode::size_t data_size = size - sizeof(Udp_packet);
		void * data = udp->data<void>();
		if (Dhcp_packet::is_dhcp(udp)) { dump_dhcp(data, data_size); }
	}

	void dump_ipv4(void * const base, Genode::size_t const size)
	{
		Ipv4_packet * ipv4 = new (base) Ipv4_packet(size);
		Genode::printf(
			"\033[32mIPV4\033[0m %u.%u.%u.%u > %u.%u.%u.%u ",
			ipv4->src().addr[0],
			ipv4->src().addr[1],
			ipv4->src().addr[2],
			ipv4->src().addr[3],
			ipv4->dst().addr[0],
			ipv4->dst().addr[1],
			ipv4->dst().addr[2],
			ipv4->dst().addr[3]);

		void * data = ipv4->data<void>();
		Genode::size_t data_size = size - sizeof(Ipv4_packet);
		switch (ipv4->protocol()) {
		case Tcp_packet::IP_ID: dump_tcp(data, data_size); break;
		case Udp_packet::IP_ID: dump_udp(data, data_size); break;
		default: ; }
	}

	void dump_arp(void * const base, Genode::size_t const size)
	{
		Arp_packet * arp = new (base) Arp_packet(size);
		if (!arp->ethernet_ipv4()) { return; }
		Genode::printf(
			"\033[32mARP\033[0m %x:%x:%x:%x:%x:%x %u.%u.%u.%u "
			"> %x:%x:%x:%x:%x:%x %u.%u.%u.%u cmd %u ",
			arp->src_mac().addr[0],
			arp->src_mac().addr[1],
			arp->src_mac().addr[2],
			arp->src_mac().addr[3],
			arp->src_mac().addr[4],
			arp->src_mac().addr[5],
			arp->src_ip().addr[0],
			arp->src_ip().addr[1],
			arp->src_ip().addr[2],
			arp->src_ip().addr[3],
			arp->dst_mac().addr[0],
			arp->dst_mac().addr[1],
			arp->dst_mac().addr[2],
			arp->dst_mac().addr[3],
			arp->dst_mac().addr[4],
			arp->dst_mac().addr[5],
			arp->dst_ip().addr[0],
			arp->dst_ip().addr[1],
			arp->dst_ip().addr[2],
			arp->dst_ip().addr[3],
			arp->opcode());
	}

	void dump_eth(void * const base, Genode::size_t const size)
	{
		Ethernet_frame * eth = new (base) Ethernet_frame(size);
		Genode::printf(
			"\033[32mETH\033[0m %x:%x:%x:%x:%x:%x > %x:%x:%x:%x:%x:%x ",
			eth->src().addr[0],
			eth->src().addr[1],
			eth->src().addr[2],
			eth->src().addr[3],
			eth->src().addr[4],
			eth->src().addr[5],
			eth->dst().addr[0],
			eth->dst().addr[1],
			eth->dst().addr[2],
			eth->dst().addr[3],
			eth->dst().addr[4],
			eth->dst().addr[5]);

		void * data = eth->data<void>();
		Genode::size_t data_size = size - sizeof(Ethernet_frame);
		switch (eth->type()) {
		case Ethernet_frame::ARP: dump_arp(data, data_size); break;
		case Ethernet_frame::IPV4: dump_ipv4(data, data_size); break;
		default: ; }
	}
}

#endif /* _NET__DUMP_H_ */
