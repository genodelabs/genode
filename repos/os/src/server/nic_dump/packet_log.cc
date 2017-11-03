/*
 * \brief  Configurable print functionality for network packets
 * \author Martin Stein
 * \date   2017-09-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <packet_log.h>

using namespace Genode;
using namespace Net;


template <>
void Packet_log<Dhcp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.dhcp) {
	case Packet_log_style::COMPREHENSIVE:

		print(output, "\033[32mDHCP\033[0m");
		print(output, " op ",   _pkt.op());
		print(output, " htyp ", (uint8_t)_pkt.htype());
		print(output, " hlen ", _pkt.hlen());
		print(output, " hps ",  _pkt.hops());
		print(output, " xid ",  _pkt.xid());
		print(output, " sec ",  _pkt.secs());
		print(output, " flg ",  Hex(_pkt.flags()));
		print(output, " ci ",   _pkt.ciaddr());
		print(output, " yi ",   _pkt.yiaddr());
		print(output, " si ",   _pkt.siaddr());
		print(output, " gi ",   _pkt.giaddr());
		print(output, " ch ",   _pkt.client_mac());
		print(output, " srv ",  _pkt.server_name());
		print(output, " file ", _pkt.file());
		print(output, " mag ",  _pkt.magic_cookie());
		print(output, " opt");
		_pkt.for_each_option([&] (Dhcp_packet::Option &opt) {
			print(output, " ", opt);
		});
		break;

	case Packet_log_style::COMPACT:

		print(output, "\033[32mDHCP\033[0m ", _pkt.client_mac(), " > ",
		      _pkt.siaddr(), " cmd ", _pkt.op());
		break;

	case Packet_log_style::SHORT:

		print(output, "\033[32mDHCP\033[0m");
		break;

	default: ;
	}
}


void Dhcp_packet::Option::print(Output &output) const
{
	using Genode::print;

	print(output, _code, ":", _len);
	if (!len()) {
		return;
	}
	print(output, ":");
	for (unsigned j = 0; j < len(); j++) {
		print(output, Hex(_value[j], Hex::OMIT_PREFIX, Hex::PAD));
	}
}


template <>
void Packet_log<Arp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.arp) {
	case Packet_log_style::COMPREHENSIVE:

		print(output, "\033[32mARP\033[0m");
		print(output, " hw ",     _pkt.hardware_address_type());
		print(output, " prot ",   _pkt.protocol_address_type());
		print(output, " hwsz ",   _pkt.hardware_address_size());
		print(output, " protsz ", _pkt.protocol_address_size());
		print(output, " op ",     _pkt.opcode());

		if (_pkt.ethernet_ipv4()) {

			print(output, " srcmac ", _pkt.src_mac());
			print(output, " srcip ",  _pkt.src_ip());
			print(output, " dstmac ", _pkt.dst_mac());
			print(output, " dstip ",  _pkt.dst_ip());

		} else {
			print(output, " ...");
		}
		break;

	case Packet_log_style::COMPACT:

		print(output, "\033[32mARP\033[0m ", _pkt.src_mac(), " ",
		     _pkt.src_ip(), " > ", _pkt.dst_mac(), " ", _pkt.dst_ip(),
		     " cmd ", _pkt.opcode());
		break;

	case Packet_log_style::SHORT:

		print(output, "\033[32mARP\033[0m");
		break;

	default: ;
	}
}


template <>
void Packet_log<Ethernet_frame>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.eth) {
	case Packet_log_style::COMPREHENSIVE:

		print(output, "\033[32mETH\033[0m");
		print(output, " src ", _pkt.src());
		print(output, " dst ", _pkt.dst());
		print(output, " typ ", (Genode::uint16_t)_pkt.type());
		break;

	case Packet_log_style::COMPACT:

		print(output, "\033[32mETH\033[0m ", _pkt.src(), " > ", _pkt.dst(),
		      " ");
		break;

	case Packet_log_style::SHORT:

		print(output, "\033[32mETH\033[0m");
		break;

	default: ;
	}
	/* print encapsulated packet */
	switch (_pkt.type()) {
	case Ethernet_frame::Type::ARP:

		print(output, " ", packet_log(*_pkt.data<Arp_packet const>(), _cfg));
		break;

	case Ethernet_frame::Type::IPV4:

		print(output, " ", packet_log(*_pkt.data<Ipv4_packet const>(), _cfg));
		break;

	default: ;
	}
}


template <>
void Packet_log<Ipv4_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.ipv4) {
	case Packet_log_style::COMPREHENSIVE:

		print(output, "\033[32mIPV4\033[0m");
		print(output, " hdrlen ", _pkt.header_length());
		print(output, " ver ",    _pkt.version());
		print(output, " dsrv ",   _pkt.diff_service());
		print(output, " ecn ",    _pkt.ecn());
		print(output, " len ",    _pkt.total_length());
		print(output, " id ",     _pkt.identification());
		print(output, " flg ",    _pkt.flags());
		print(output, " frgoff ", _pkt.fragment_offset());
		print(output, " ttl  ",   _pkt.time_to_live());
		print(output, " prot ",   (uint8_t)_pkt.protocol());
		print(output, " crc ",    _pkt.checksum());
		print(output, " src ",    _pkt.src());
		print(output, " dst ",    _pkt.dst());
		break;

	case Packet_log_style::COMPACT:

		print(output, "\033[32mIPV4\033[0m ", _pkt.src(), " > ", _pkt.dst(),
		      " ");
		break;

	case Packet_log_style::SHORT:

		print(output, "\033[32mIPV4\033[0m");
		break;

	default: ;
	}
	/* print encapsulated packet */
	switch (_pkt.protocol()) {
	case Ipv4_packet::Protocol::TCP:

		print(output, " ", packet_log(*_pkt.data<Tcp_packet const>(), _cfg));
		break;

	case Ipv4_packet::Protocol::UDP:

		print(output, " ", packet_log(*_pkt.data<Udp_packet const>(), _cfg));
		break;

	default: ; }
}


template <>
void Packet_log<Tcp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.tcp) {
	case Packet_log_style::COMPREHENSIVE:

		print(output, "\033[32mTCP\033[0m");
		print(output, " src ",   _pkt.src_port());
		print(output, " dst ",   _pkt.dst_port());
		print(output, " seqn ",  _pkt.seq_nr());
		print(output, " ackn ",  _pkt.ack_nr());
		print(output, " doff ",  _pkt.data_offset());
		print(output, " flg ",   _pkt.flags());
		print(output, " winsz ", _pkt.window_size());
		print(output, " crc ",   _pkt.checksum());
		print(output, " urgp ",  _pkt.urgent_ptr());
		break;

	case Packet_log_style::COMPACT:

		print(output, "\033[32mTCP\033[0m ", _pkt.src_port(), " > ",
		      _pkt.dst_port(), " flags '");
		break;

	case Packet_log_style::SHORT:

		print(output, "\033[32mTCP\033[0m");
		break;

	default: ;
	}
}


template <>
void Packet_log<Udp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.udp) {
	case Packet_log_style::COMPREHENSIVE:

		print(output, "\033[32mUDP\033[0m");
		print(output, " src ", _pkt.src_port());
		print(output, " dst ", _pkt.dst_port());
		print(output, " len ", _pkt.length());
		print(output, " crc ", _pkt.checksum());
		break;

	case Packet_log_style::COMPACT:

		print(output, "\033[32mUDP\033[0m ", _pkt.src_port(), " > ",
		      _pkt.dst_port(), " ");
		break;

	case Packet_log_style::SHORT:

		print(output, "\033[32mUDP\033[0m");
		break;

	default: ;
	}
	/* print encapsulated packet */
	if (Dhcp_packet::is_dhcp(&_pkt)) {
		print(output, " ", packet_log(*_pkt.data<Dhcp_packet const>(), _cfg));
	}
}
