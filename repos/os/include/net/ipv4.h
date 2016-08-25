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
#include <util/token.h>

#include <util/endian.h>
#include <net/netaddress.h>

namespace Net
{
	enum { IPV4_ADDR_LEN = 4 };
	typedef Network_address<IPV4_ADDR_LEN, '.', false> Ipv4_address;

	class Ipv4_address_prefix;

	class Ipv4_packet;
}


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
			ADDR_LEN = IPV4_ADDR_LEN, /* Ip address length in bytes */
		};

		static const Ipv4_address CURRENT;    /* current network   */
		static const Ipv4_address BROADCAST;  /* broadcast address */

		static Ipv4_address ip_from_string(const char *ip);

		static Genode::uint16_t calculate_checksum(Ipv4_packet const &packet);

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

		Genode::uint8_t version() { return _version; }

		/* returns the number of 32-bit words the header occupies */
		Genode::uint8_t header_length() { return _header_length; }
		Genode::uint8_t precedence()    { return _diff_service & PRECEDENCE; }

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

		Genode::uint16_t checksum() { return host_to_big_endian(_header_checksum); }

		Ipv4_address dst() { return Ipv4_address(&_dst_addr); }
		Ipv4_address src() { return Ipv4_address(&_src_addr); }

		template <typename T> T const * header() const { return (T const *)(this); }
		template <typename T> T *       data()         { return (T *)(_data); }
		template <typename T> T const * data()   const { return (T const *)(_data); }

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


struct Net::Ipv4_address_prefix
{
	Ipv4_address    address;
	Genode::uint8_t prefix = 0;
};


namespace Genode {

	inline size_t ascii_to(char const *s, Net::Ipv4_address &result);

	inline size_t ascii_to(char const *s, Net::Ipv4_address_prefix &result);
}


Genode::size_t Genode::ascii_to(char const *s, Net::Ipv4_address &result)
{
	using namespace Net;

	struct Scanner_policy_number
	{
		static bool identifier_char(char c, unsigned  i ) {
			return Genode::is_digit(c) && c !='.'; }
	};

	typedef ::Genode::Token<Scanner_policy_number> Token;

	Ipv4_address  ip_addr;
	Token         t(s);
	char          tmpstr[4];
	int           cnt = 0;
	unsigned char ipb[4] = {0};
	size_t        size = 0;

	while(t) {
		if (t.type() == Token::WHITESPACE || t[0] == '.') {
			t = t.next();
			size++;
			continue;
		}
		t.string(tmpstr, sizeof(tmpstr));

		unsigned long tmpc = 0;
		size += Genode::ascii_to(tmpstr, tmpc);
		ipb[cnt] = tmpc & 0xFF;
		t = t.next();

		if (cnt == 4)
			break;
		cnt++;
	}

	if (cnt == 4) {
		result.addr[0] = ipb[0];
		result.addr[1] = ipb[1];
		result.addr[2] = ipb[2];
		result.addr[3] = ipb[3];
		return size;
	}

	return 0;
}


Genode::size_t Genode::ascii_to(char const *s, Net::Ipv4_address_prefix &result)
{
	size_t size = ascii_to(s, result.address);
	if (!size || s[size] != '/') { return 0; }
	char const * prefix = &s[size + 1];
	size_t const prefix_size = ascii_to_unsigned(prefix, result.prefix, 10);
	if (!prefix_size) { return 0; }
	size += prefix_size + 1;
	return size;
}

#endif /* _IPV4_H_ */
