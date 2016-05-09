/*
 * \brief  Internet protocol version 4.
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IPV4_H_
#define _IPV4_H_

/* Genode */
#include <base/exception.h>
#include <util/string.h>

#include <util/endian.h>
#include <net/netaddress.h>

namespace Net { class Ipv4_packet; }


/**
 * Data layout of this class conforms to an IPv4 packet (RFC 791)
 *
 * IPv4-header-format:
 *
 *  ----------------------------------------------------------------
 * |   0-3   |  4-7  | 8-11 | 12-15 | 16-18 | 19-23 | 24-27 | 28-31 |
 *  ----------------------------------------------------------------
 * | version |  IHL  | service-type |         total-length          |
 *  ----------------------------------------------------------------
 * |         identifikation         | flags |     fragment-offset   |
 *  ----------------------------------------------------------------
 * |        ttl      |   protocol   |       header-checksum         |
 *  ----------------------------------------------------------------
 * |                       source-ip-address                        |
 *  ----------------------------------------------------------------
 * |                     destination-ip-address                     |
 *  ----------------------------------------------------------------
 * |                            options ...                         |
 *  ----------------------------------------------------------------
 */
class Net::Ipv4_packet
{
	public:

		enum Size {
			ADDR_LEN = 4, /* Ip address length in bytes */
		};

		typedef Network_address<ADDR_LEN> Ipv4_address;

		static const Ipv4_address CURRENT;    /* current network   */
		static const Ipv4_address BROADCAST;  /* broadcast address */

		static Ipv4_address ip_from_string(const char *ip);

		static Genode::uint16_t calculate_checksum(const Ipv4_packet &packet);

	private:

		/************************
		 ** IPv4 header fields **
		 ************************/

		unsigned         _header_length   : 4;
		unsigned         _version         : 4;
		Genode::uint8_t  _diff_service;
		Genode::uint16_t _total_length;
		Genode::uint16_t _identification;
		unsigned         _flags           : 3;
		unsigned         _fragment_offset : 13;
		Genode::uint8_t  _time_to_live;
		Genode::uint8_t  _protocol;
		Genode::uint16_t _header_checksum;
		Genode::uint8_t  _src_addr[ADDR_LEN];
		Genode::uint8_t  _dst_addr[ADDR_LEN];
		unsigned         _data[0];

		/**
		 * Bitmasks for differentiated services field.
		 */
		enum Differentiated_services {
			PRECEDENCE  = 0x7,
			DELAY       = 0x8,
			THROUGHPUT  = 0x10,
			RELIABILITY = 0x20,
			COST        = 0x40
		};

	public:

		enum Precedence {
			NETWORK_CONTROL      = 7,
			INTERNETWORK_CONTROL = 6,
			CRITIC_ECP           = 5,
			FLASH_OVERRIDE       = 4,
			FLASH                = 3,
			IMMEDIATE            = 2,
			PRIORITY             = 1,
			ROUTINE              = 0
		};

		enum Flags {
			NO_FRAGMENT    = 0x2,
			MORE_FRAGMENTS = 0x4
		};


		/**
		 * Exception used to indicate protocol violation.
		 */
		class No_ip_packet : Genode::Exception {};


		/*****************
		 ** Constructor **
		 *****************/

		Ipv4_packet(Genode::size_t size) {
			/* ip header needs to fit in */
			if (size < sizeof(Ipv4_packet))
				throw No_ip_packet();
		}


		/*******************************
		 ** IPv4 field read-accessors **
		 *******************************/

		Genode::size_t  version()     { return _version;                   }
		Genode::size_t  header_length() { return _header_length;           }
		Genode::uint8_t precedence()  { return _diff_service & PRECEDENCE; }

		bool low_delay()              { return _diff_service & DELAY;      }
		bool high_throughput()        { return _diff_service & THROUGHPUT; }
		bool high_reliability()       { return _diff_service & RELIABILITY;}
		bool minimize_monetary_cost() { return _diff_service & COST;       }

		Genode::uint16_t total_length()   { return host_to_big_endian(_total_length);   }
		Genode::uint16_t identification() { return host_to_big_endian(_identification); }

		bool no_fragmentation() { return _flags & NO_FRAGMENT;             }
		bool more_fragments()   { return _flags & MORE_FRAGMENTS;          }

		Genode::size_t   fragment_offset() { return _fragment_offset;      }
		Genode::uint8_t  time_to_live()    { return _time_to_live;         }
		Genode::uint8_t  protocol()        { return _protocol;             }

		Genode::uint16_t checksum() { return host_to_big_endian(_header_checksum);      }

		Ipv4_address dst() { return Ipv4_address(&_dst_addr); }
		Ipv4_address src() { return Ipv4_address(&_src_addr); }

		void *data() { return &_data; }

		/********************************
		 ** IPv4 field write-accessors **
		 ********************************/

		void version(Genode::size_t version)    { _version = version; }
		void header_length(Genode::size_t len)  { _header_length = len; }

		void total_length(Genode::uint16_t len) { _total_length = host_to_big_endian(len); }
		void time_to_live(Genode::uint8_t ttl)  { _time_to_live = ttl; }

		void checksum(Genode::uint16_t checksum) { _header_checksum = host_to_big_endian(checksum); }

		void dst(Ipv4_address ip) { ip.copy(&_dst_addr); }
		void src(Ipv4_address ip) { ip.copy(&_src_addr); }

		/***************
		 ** Operators **
		 ***************/

		/**
		 * Placement new.
		 */
		void * operator new(Genode::size_t size, void* addr) {
			return addr; }

} __attribute__((packed));

#endif /* _IPV4_H_ */
