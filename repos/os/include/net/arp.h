/*
 * \brief  Address resolution protocol
 * \author Stefan Kalkowski
 * \date   2010-08-24
 *
 * ARP is used to determine a network host's link layer or
 * hardware address when only its Network Layer address is known.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

		/***********************
		 ** ARP header fields **
		 ***********************/

		Genode::uint16_t _hw_addr_type;
		Genode::uint16_t _prot_addr_type;
		Genode::uint8_t  _hw_addr_sz;
		Genode::uint8_t  _prot_addr_sz;
		Genode::uint16_t _opcode;
		Genode::uint8_t  _src_mac_addr[Ethernet_frame::ADDR_LEN];
		Genode::uint8_t  _src_ip_addr[Ipv4_packet::ADDR_LEN];
		Genode::uint8_t  _dst_mac_addr[Ethernet_frame::ADDR_LEN];
		Genode::uint8_t  _dst_ip_addr[Ipv4_packet::ADDR_LEN];

	public:

		/********************
		 ** ARP parameters **
		 ********************/

		enum Protocol_address_type { IPV4 = 0x0800 };

		enum Hardware_type {
			ETHERNET                = 0x0001,
			EXP_ETHERNET            = 0x0002,
			AX_25                   = 0x0003,
			TOKEN_RING              = 0x0004,
			CHAOS                   = 0x0005,
			IEEE802_NET             = 0x0006,
			ARCNET                  = 0x0007,
			HYPERCHANNEL            = 0x0008,
			LANSTAR                 = 0x0009,
			AUTONET                 = 0x000A,
			LOCALTALK               = 0x000B,
			LOCALNET                = 0x000C,
			ULTRA_LINK              = 0x000D,
			SMDS                    = 0x000E,
			FRAME_RELAY             = 0x000F,
			ATM_1                   = 0x0010,
			HDLC                    = 0x0011,
			FIBRE_CHANNEL           = 0x0012,
			ATM_2                   = 0x0013,
			SERIAL_LINE             = 0x0014,
			ATM_3                   = 0x0015,
			MIL_STD_188_220         = 0x0016,
			METRICOM                = 0x0017,
			IEEE1394                = 0x0018,
			MAPOS                   = 0x0019,
			TWINAXIAL               = 0x001A,
			EUI_64                  = 0x001B,
			HIPARP                  = 0x001C,
			IP_AND_ARP_OVER_ISO7816 = 0x001D,
			ARPSEC                  = 0x001E,
			IPSEC_TUNNEL            = 0x001F,
			INFINIBAND              = 0x0020,
			TIA_102                 = 0x0021,
			WIEGAND_INTERFACE       = 0x0022,
			Pure_IP                 = 0x0023,
			HW_EXP1                 = 0x0024,
			HFI                     = 0x0025,
			HW_EXP2                 = 0x0100,
		};

		enum Opcode {
			REQUEST                 = 0x0001,
			REPLY                   = 0x0002,
			REQUEST_REVERSE         = 0x0003,
			REPLY_REVERSE           = 0x0004,
			DRARP_REQUEST           = 0x0005,
			DRARP_REPLY             = 0x0006,
			DRARP_ERROR             = 0x0007,
			INARP_REQUEST           = 0x0008,
			INARP_REPLY             = 0x0009,
			ARP_NAK                 = 0x000A,
			MARS_REQUEST            = 0x000B,
			MARS_MULTI              = 0x000C,
			MARS_MSERV              = 0x000D,
			MARS_JOIN               = 0x000E,
			MARS_LEAVE              = 0x000F,
			MARS_NAK                = 0x0010,
			MARS_UNSERV             = 0x0011,
			MARS_SJOIN              = 0x0012,
			MARS_SLEAVE             = 0x0013,
			MARS_GROUPLIST_REQUEST  = 0x0014,
			MARS_GROUPLIST_REPLY    = 0x0015,
			MARS_REDIRECT_MAP       = 0x0016,
			MAPOS_UNARP             = 0x0017,
			OP_EXP1                 = 0x0018,
			OP_EXP2                 = 0x0019
		};


		/**
		 * Exception used to indicate protocol violation.
		 */
		class No_arp_packet : Genode::Exception {};


		/*****************
		 ** Constructor **
		 *****************/

		Arp_packet(Genode::size_t size) {
			/* arp packet needs to fit in */
			if (size < sizeof(Arp_packet))
				throw No_arp_packet();
		}


		/******************************
		 ** ARP field read-accessors **
		 ******************************/

		/**
		 * \return link layer type (Arp_packet::Hardware_type).
		 */
		Genode::uint16_t hardware_address_type() {
			return host_to_big_endian(_hw_addr_type); }

		/**
		 * \return network/internet layer type (Ether_frame::EtherType).
		 */
		Genode::uint16_t protocol_address_type() {
			return host_to_big_endian(_prot_addr_type); }

		/**
		 * \return size in bytes of hardware address.
		 */
		Genode::uint8_t hardware_address_size() { return _hw_addr_sz; }

		/**
		 * \return size in bytes of protocol address.
		 */
		Genode::uint8_t protocol_address_size() { return _prot_addr_sz; }

		/**
		 * \return operation code (Arp_packet::Opcode).
		 */
		Genode::uint16_t opcode() const { return host_to_big_endian(_opcode); }

		/**
		 * \return source MAC address.
		 */
		Mac_address src_mac() const {
			return Mac_address((void *)&_src_mac_addr); }

		/**
		 * \return source IP address.
		 */
		Ipv4_address src_ip() const {
			return Ipv4_address((void *)&_src_ip_addr); }

		/**
		 * \return destination MAC address.
		 */
		Mac_address dst_mac() const {
			return Mac_address((void *)&_dst_mac_addr); }

		/**
		 * \return destination IP address.
		 */
		Ipv4_address dst_ip() const {
			return Ipv4_address((void *)&_dst_ip_addr); }


		/******************************
		 ** ARP field write-accessors **
		 ******************************/

		/**
		 * \return link layer type (Arp_packet::Hardware_type).
		 */
		void hardware_address_type(Genode::uint16_t v) {
			_hw_addr_type = host_to_big_endian(v); }

		/**
		 * \return network/internet layer type (Ether_frame::EtherType).
		 */
		void protocol_address_type(Genode::uint16_t v) {
			_prot_addr_type = host_to_big_endian(v); }

		/**
		 * \return size in bytes of hardware address.
		 */
		void hardware_address_size(Genode::uint8_t v) { _hw_addr_sz = v; }

		/**
		 * \return size in bytes of protocol address.
		 */
		void protocol_address_size(Genode::uint8_t v) { _prot_addr_sz = v; }

		/**
		 * Set Operation code.
		 *
		 * \param opcode  Arp_packet::Opcode to set.
		 */
		void opcode(Genode::uint16_t opcode) {
			_opcode = host_to_big_endian(opcode); }

		/**
		 * Set source MAC address.
		 *
		 * \param src_mac_addr  MAC address to set.
		 */
		void src_mac(Mac_address src_mac_addr) {
			src_mac_addr.copy(&_src_mac_addr); }

		/**
		 * Set source IP address.
		 *
		 * \param src_ip_addr  IP address to set.
		 */
		void src_ip(Ipv4_address src_ip_addr) {
			src_ip_addr.copy(&_src_ip_addr); }

		/**
		 * Set destination MAC address.
		 *
		 * \param src_mac_addr  MAC address to set.
		 */
		void dst_mac(Mac_address dst_mac_addr) {
			dst_mac_addr.copy(&_dst_mac_addr); }

		/**
		 * Set destination IP address.
		 *
		 * \param src_ip_addr  IP address to set.
		 */
		void dst_ip(Ipv4_address dst_ip_addr) {
			dst_ip_addr.copy(&_dst_ip_addr); }


		/***************************
		 ** Convenience functions **
		 ***************************/

		/**
		 * \return true when ARP packet really targets ethernet
		 *         address resolution with respect to IPv4 addresses.
		 */
		bool ethernet_ipv4() const {
			return (   host_to_big_endian(_hw_addr_type)   == ETHERNET
			        && host_to_big_endian(_prot_addr_type) == Ethernet_frame::IPV4
			        && _hw_addr_sz   == Ethernet_frame::ADDR_LEN
			        && _prot_addr_sz == Ipv4_packet::ADDR_LEN);
		}


		/***************
		 ** Operators **
		 ***************/

		/**
		 * Placement new
		 */
		void * operator new(__SIZE_TYPE__ size, void* addr) { return addr; }


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _NET__ARP_H_ */
