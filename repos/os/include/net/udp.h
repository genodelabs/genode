/*
 * \brief  User datagram protocol.
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UDP_H_
#define _UDP_H_

/* Genode */
#include <base/exception.h>
#include <base/stdint.h>
#include <net/port.h>
#include <util/endian.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

namespace Net { class Udp_packet; }


/**
 * Data layout of this class conforms to an UDP packet (RFC 768)
 *
 * UDP-header-format:
 *
 *  -----------------------------------------------------------------------
 * |   source-port   | destination-port |     length      |    checksum    |
 * |     2 bytes     |     2 bytes      |     2 bytes     |    2 bytes     |
 *  -----------------------------------------------------------------------
 */
class Net::Udp_packet
{
	private:

		/***********************
		 ** UDP header fields **
		 ***********************/

		Genode::uint16_t _src_port;
		Genode::uint16_t _dst_port;
		Genode::uint16_t _length;
		Genode::uint16_t _checksum;
		unsigned         _data[0];

	public:

		/**
		 * Exception used to indicate protocol violation.
		 */
		class No_udp_packet : Genode::Exception {};


		/*****************
		 ** Constructor **
		 *****************/

		Udp_packet(Genode::size_t size) {
			/* Udp header needs to fit in */
			if (size < sizeof(Udp_packet))
				throw No_udp_packet();
		}


		/***************
		 ** Accessors **
		 ***************/

		Port                            src_port() const { return Port(host_to_big_endian(_src_port)); }
		Port                            dst_port() const { return Port(host_to_big_endian(_dst_port)); }
		Genode::uint16_t                length()   const { return host_to_big_endian(_length);   }
		Genode::uint16_t                checksum() const { return host_to_big_endian(_checksum); }
		template <typename T> T const * data()     const { return (T const *)(_data); }
		template <typename T> T *       data()           { return (T *)(_data); }

		void length(Genode::uint16_t v) { _length = host_to_big_endian(v); }
		void src_port(Port p)           { _src_port = host_to_big_endian(p.value); }
		void dst_port(Port p)           { _dst_port = host_to_big_endian(p.value); }


		/***************
		 ** Operators **
		 ***************/

		/**
		 * Placement new.
		 */
		void * operator new(__SIZE_TYPE__, void* addr) { return addr; }


		/***************************
		 ** Convenience functions **
		 ***************************/

		/**
		 * UDP checksum is calculated over the udp datagram + an IPv4
		 * pseudo header.
		 *
		 * IPv4 pseudo header:
		 *
		 *  --------------------------------------------------------------
		 * | src-ipaddr | dst-ipaddr | zero-field | prot.-id | udp-length |
		 * |  4 bytes   |  4 bytes   |   1 byte   |  1 byte  |  2 bytes   |
		 *  --------------------------------------------------------------
		 */
		void update_checksum(Ipv4_address src,
		                     Ipv4_address dst)
		{
			/* have to reset the checksum field for calculation */
			_checksum = 0;

			/*
			 * sum up pseudo header
			 */
			Genode::uint32_t sum = 0;
			for (unsigned i = 0; i < Ipv4_packet::ADDR_LEN; i=i+2) {
				Genode::uint16_t s = src.addr[i] << 8 | src.addr[i + 1];
				Genode::uint16_t d = dst.addr[i] << 8 | dst.addr[i + 1];
				sum += s + d;
			}
			Genode::uint8_t prot[] = { 0, (Genode::uint8_t)Ipv4_packet::Protocol::UDP };
			sum += host_to_big_endian(*(Genode::uint16_t*)&prot) + length();

			/*
			 * sum up udp packet itself
			 */
			unsigned max = (length() & 1) ? (length() - 1) : length();
			Genode::uint16_t *udp = (Genode::uint16_t*) this;
			for (unsigned i = 0; i < max; i=i+2)
				sum += host_to_big_endian(*udp++);

			/* if udp length is odd, append a zero byte */
			if (length() & 1) {
				Genode::uint8_t last[] =
					{ *((Genode::uint8_t*)this + (length()-1)), 0 };
				sum += host_to_big_endian(*(Genode::uint16_t*)&last);
			}

			/*
			 * keep only the last 16 bits of the 32 bit calculated sum
			 * and add the carries
			 */
			while (sum >> 16)
				sum = (sum & 0xffff) + (sum >> 16);

			/* one's complement of sum */
			_checksum = host_to_big_endian((Genode::uint16_t) ~sum);
		}


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _UDP_H_ */
