/*
 * \brief  Internet protocol version 4
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/token.h>
#include <util/string.h>

#include <net/udp.h>
#include <net/tcp.h>
#include <net/ipv4.h>

using namespace Genode;
using namespace Net;


void Net::Ipv4_packet::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mIPV4\033[0m ", src(), " > ", dst(), " ");
	switch (protocol()) {
	case Protocol::TCP:
		Genode::print(output,
		              *reinterpret_cast<Tcp_packet const *>(data<void>()));
		break;
	case Protocol::UDP:
		Genode::print(output,
		              *reinterpret_cast<Udp_packet const *>(data<void>()));
		break;
	default: ; }
}


bool Ipv4_address::is_in_range(Ipv4_address const &first,
                               Ipv4_address const &last) const
{
	uint32_t const ip_raw = to_uint32_little_endian();
	return ip_raw >= first.to_uint32_little_endian() &&
	       ip_raw <= last.to_uint32_little_endian();
}


uint32_t Ipv4_address::to_uint32_big_endian() const
{
	return addr[0] |
	       addr[1] << 8 |
	       addr[2] << 16 |
	       addr[3] << 24;
}


Ipv4_address Ipv4_address::from_uint32_big_endian(uint32_t ip_raw)
{
	Ipv4_address ip;
	ip.addr[0] = ip_raw;
	ip.addr[1] = ip_raw >> 8;
	ip.addr[2] = ip_raw >> 16;
	ip.addr[3] = ip_raw >> 24;
	return ip;
}


uint32_t Ipv4_address::to_uint32_little_endian() const
{
	return addr[3] |
	       addr[2] << 8 |
	       addr[1] << 16 |
	       addr[0] << 24;
}


Ipv4_address Ipv4_address::from_uint32_little_endian(uint32_t ip_raw)
{
	Ipv4_address ip;
	ip.addr[3] = ip_raw;
	ip.addr[2] = ip_raw >> 8;
	ip.addr[1] = ip_raw >> 16;
	ip.addr[0] = ip_raw >> 24;
	return ip;
}


struct Scanner_policy_number
{
	static bool identifier_char(char c, unsigned  i ) {
		return Genode::is_digit(c) && c !='.'; }
};


Ipv4_address Ipv4_packet::ip_from_string(const char *ip)
{
	using Token = ::Genode::Token<Scanner_policy_number>;

	Ipv4_address  ip_addr;
	Token         t(ip);
	char          tmpstr[4];
	int           cnt = 0;
	unsigned char ipb[4] = {0};

	while(t) {
		if (t.type() == Token::WHITESPACE || t[0] == '.') {
			t = t.next();
			continue;
		}
		t.string(tmpstr, sizeof(tmpstr));

		unsigned long tmpc = 0;
		Genode::ascii_to(tmpstr, tmpc);
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

Genode::uint16_t Ipv4_packet::calculate_checksum(Ipv4_packet const &packet)
{
	Genode::uint16_t const *data = (Genode::uint16_t *)&packet;
	Genode::uint32_t const sum = host_to_big_endian(data[0])
	                           + host_to_big_endian(data[1])
	                           + host_to_big_endian(data[2])
	                           + host_to_big_endian(data[3])
	                           + host_to_big_endian(data[4])
	                           + host_to_big_endian(data[6])
	                           + host_to_big_endian(data[7])
	                           + host_to_big_endian(data[8])
	                           + host_to_big_endian(data[9]);
	return ~((0xFFFF & sum) + (sum >> 16));
}


const Ipv4_address Ipv4_packet::CURRENT((Genode::uint8_t)0x00);
const Ipv4_address Ipv4_packet::BROADCAST((Genode::uint8_t)0xFF);
