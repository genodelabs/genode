/*
 * \brief  Configurable print functionality for network packets
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2017-09-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PACKET_LOG_H_
#define _PACKET_LOG_H_

/* Genode includes */
#include <base/stdint.h>
#include <net/arp.h>
#include <net/udp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/tcp.h>
#include <net/icmp.h>
#include <net/ipv4.h>

namespace Net {

	enum class Packet_log_style : Genode::uint8_t
	{
		NO, NAME, DEFAULT, ALL,
	};

	struct Packet_log_config;

	template <typename PKT>
	struct Packet_log;
}

namespace Genode
{
	inline size_t ascii_to(char const *s, Net::Packet_log_style &result)
	{
		if (!strcmp(s, "no",      2)) { result = Net::Packet_log_style::NO;      return 2; }
		if (!strcmp(s, "name",    4)) { result = Net::Packet_log_style::NAME;    return 4; }
		if (!strcmp(s, "default", 7)) { result = Net::Packet_log_style::DEFAULT; return 7; }
		if (!strcmp(s, "all",     3)) { result = Net::Packet_log_style::ALL;     return 3; }
		return 0;
	}
}


/**
 * Configuration for the print functionality of network packets
 */
struct Net::Packet_log_config
{
	using Style = Packet_log_style;

	Style eth, arp, ipv4, dhcp, udp, icmp, tcp;

	Packet_log_config(Style def = Style::DEFAULT)
	: eth(def), arp(def), ipv4(def), dhcp(def), udp(def), icmp(def), tcp(def) { }

	Packet_log_config(Style eth,
	                  Style arp,
	                  Style ipv4,
	                  Style dhcp,
	                  Style udp,
	                  Style icmp,
	                  Style tcp)
	: eth(eth), arp(arp), ipv4(ipv4), dhcp(dhcp), udp(udp), icmp(icmp), tcp(tcp) { }
};


/**
 * Wrapper for network packets to configure their print functionality
 */
template <typename PKT>
class Net::Packet_log
{
	private:

		PKT               const &_pkt;
		Packet_log_config const &_cfg;

	public:

		Packet_log(PKT const &pkt, Packet_log_config const &cfg)
		: _pkt(pkt), _cfg(cfg) { }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;
};


namespace Net {

	/* getting an instances via a function enables template-arg deduction */
	template <typename PKT>
	Packet_log<PKT> packet_log(PKT const &pkt, Packet_log_config const &cfg) {
		return Packet_log<PKT>(pkt, cfg); }


	/************************************************************
	 ** Packet log specializations for all supported protocols **
	 ************************************************************/

	template <>
	void Packet_log<Arp_packet>::print(Genode::Output &output) const;

	template <>
	void Packet_log<Ethernet_frame>::print(Genode::Output &output) const;

	template <>
	void Packet_log<Dhcp_packet>::print(Genode::Output &output) const;

	template <>
	void Packet_log<Ipv4_packet>::print(Genode::Output &output) const;

	template <>
	void Packet_log<Tcp_packet>::print(Genode::Output &output) const;

	template <>
	void Packet_log<Udp_packet>::print(Genode::Output &output) const;

	template <>
	void Packet_log<Icmp_packet>::print(Genode::Output &output) const;
}

#endif /* _PACKET_LOG_H_ */
