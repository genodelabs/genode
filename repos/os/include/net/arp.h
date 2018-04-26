/*
 * \brief  Address resolution protocol
 * \author Stefan Kalkowski
 * \date   2010-08-24
 *
 * ARP is used to determine a network host's link layer or
 * hardware address when only its Network Layer address is known.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__ARP_H_
#define _NET__ARP_H_

/* Genode */
#include <base/exception.h>
#include <base/stdint.h>

#include <util/endian.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

namespace Net { class Arp_packet; }


/**
 * Data layout of this class conforms to an ARP-packet (RFC 826)
 *
 * It's reduced to Internet protocol (IPv4) over Ethernet.
 *
 * ARP-packet-format:
 *
 *  --------------------------------------------------------------------
 * |    Bit 0-7    |    Bit 8-15    |    Bit 16-23   |     Bit 24-31    |
 *  --------------------------------------------------------------------
 * |          hw.addr.type          |          prot.addr.type           |
 *  --------------------------------------------------------------------
 * | hw.addr.size  | prot.addr.size |            operation              |
 *  --------------------------------------------------------------------
 * |                        source-mac-address                          |
 *  --------------------------------------------------------------------
 * |       source-mac-address       |          source-ip-address        |
 *  --------------------------------------------------------------------
 * |       source-ip-address        |          dest.-mac-address        |
 *  --------------------------------------------------------------------
 * |                         dest.-mac-address                          |
 *  --------------------------------------------------------------------
 * |                         dest.-ip-address                           |
 *  --------------------------------------------------------------------
 */
class Net::Arp_packet
{
	private:

		Genode::uint16_t _hardware_address_type;
		Genode::uint16_t _protocol_address_type;
		Genode::uint8_t  _hardware_address_size;
		Genode::uint8_t  _protocol_address_size;
		Genode::uint16_t _opcode;
		Genode::uint8_t  _src_mac[Ethernet_frame::ADDR_LEN];
		Genode::uint8_t  _src_ip[Ipv4_packet::ADDR_LEN];
		Genode::uint8_t  _dst_mac[Ethernet_frame::ADDR_LEN];
		Genode::uint8_t  _dst_ip[Ipv4_packet::ADDR_LEN];

	public:

		enum Protocol_address_type {
			IPV4 = 0x0800,
		};

		enum Hardware_type {
			ETHERNET = 0x0001,
		};

		enum Opcode {
			REQUEST = 0x0001,
			REPLY   = 0x0002,
		};


		/***************
		 ** Accessors **
		 ***************/

		Genode::uint16_t hardware_address_type() const { return host_to_big_endian(_hardware_address_type); }
		Genode::uint16_t protocol_address_type() const { return host_to_big_endian(_protocol_address_type); }
		Genode::uint8_t  hardware_address_size() const { return _hardware_address_size; }
		Genode::uint8_t  protocol_address_size() const { return _protocol_address_size; }
		Genode::uint16_t opcode()                const { return host_to_big_endian(_opcode); }
		Mac_address      src_mac()               const { return Mac_address((void *)&_src_mac); }
		Ipv4_address     src_ip()                const { return Ipv4_address((void *)&_src_ip); }
		Mac_address      dst_mac()               const { return Mac_address((void *)&_dst_mac); }
		Ipv4_address     dst_ip()                const { return Ipv4_address((void *)&_dst_ip); }

		void hardware_address_type(Genode::uint16_t v) { _hardware_address_type = host_to_big_endian(v); }
		void protocol_address_type(Genode::uint16_t v) { _protocol_address_type = host_to_big_endian(v); }
		void hardware_address_size(Genode::uint8_t v)  { _hardware_address_size = v; }
		void protocol_address_size(Genode::uint8_t v)  { _protocol_address_size = v; }
		void opcode(Genode::uint16_t v)                { _opcode = host_to_big_endian(v); }
		void src_mac(Mac_address v)                    { v.copy(&_src_mac); }
		void src_ip(Ipv4_address v)                    { v.copy(&_src_ip); }
		void dst_mac(Mac_address v)                    { v.copy(&_dst_mac); }
		void dst_ip(Ipv4_address v)                    { v.copy(&_dst_ip); }


		/***************************
		 ** Convenience functions **
		 ***************************/

		/**
		 * \return true when ARP packet really targets ethernet
		 *         address resolution with respect to IPv4 addresses.
		 */
		bool ethernet_ipv4() const {
			return (   host_to_big_endian(_hardware_address_type) == ETHERNET
			        && host_to_big_endian(_protocol_address_type) == (Genode::uint16_t)Ethernet_frame::Type::IPV4
			        && _hardware_address_size == Ethernet_frame::ADDR_LEN
			        && _protocol_address_size == Ipv4_packet::ADDR_LEN);
		}

		Genode::size_t size(Genode::size_t max_size) const
		{
			return sizeof(Arp_packet) < max_size ? sizeof(Arp_packet) : max_size;
		}


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _NET__ARP_H_ */
