/*
 * \brief  User datagram protocol.
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UDP_H_
#define _UDP_H_

/* Genode */
#include <base/exception.h>
#include <base/stdint.h>

#include <util/endian.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

namespace Net {

	/**
	 * The data-layout of this class conforms to an UDP packet (RFC 768).
	 *
	 * UDP-header-format:
	 *
	 *  -----------------------------------------------------------------------
	 * |   source-port   | destination-port |     length      |    checksum    |
	 * |     2 bytes     |     2 bytes      |     2 bytes     |    2 bytes     |
	 *  -----------------------------------------------------------------------
	 */
	class Udp_packet
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

			enum Protocol_id { IP_ID = 0x11 };


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


			/******************************
			 ** UDP field read-accessors **
			 ******************************/

			Genode::uint16_t src_port()  { return bswap(_src_port); }
			Genode::uint16_t dst_port()  { return bswap(_dst_port); }
			Genode::uint16_t length()    { return bswap(_length);   }
			Genode::uint16_t checksum()  { return bswap(_checksum); }
			void* data()                 { return &_data;           }


			/***************
			 ** Operators **
			 ***************/

			/**
			 * Placement new.
			 */
			void * operator new(Genode::size_t size, void* addr) {
				return addr; }


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
			void calc_checksum(Ipv4_packet::Ipv4_address src,
			                   Ipv4_packet::Ipv4_address dst)
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
				Genode::uint8_t prot[] = { 0, IP_ID };
				sum += bswap(*(Genode::uint16_t*)&prot) + length();

				/*
				 * sum up udp packet itself
				 */
				unsigned max = (length() & 1) ? (length() - 1) : length();
				Genode::uint16_t *udp = (Genode::uint16_t*) this;
				for (unsigned i = 0; i < max; i=i+2)
					sum += bswap(*udp++);

				/* if udp length is odd, append a zero byte */
				if (length() & 1) {
					Genode::uint8_t last[] =
						{ *((Genode::uint8_t*)this + (length()-1)), 0 };
					sum += bswap(*(Genode::uint16_t*)&last);
				}

				/*
				 * keep only the last 16 bits of the 32 bit calculated sum
				 * and add the carries
				 */
				while (sum >> 16)
					sum = (sum & 0xffff) + (sum >> 16);

				/* one's complement of sum */
				_checksum = bswap((Genode::uint16_t) ~sum);
			}
	} __attribute__((packed));
}

#endif /* _UDP_H_ */
