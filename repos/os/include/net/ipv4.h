/*
 * \brief  Internet protocol version 4.
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _IPV4_H_
#define _IPV4_H_

/* Genode */
#include <base/exception.h>
#include <util/string.h>
#include <util/token.h>

#include <util/endian.h>
#include <net/netaddress.h>

namespace Genode { class Output; }

namespace Net
{
	enum { IPV4_ADDR_LEN = 4 };

	class Ipv4_address;

	class Ipv4_address_prefix;

	class Ipv4_packet;
}


struct Net::Ipv4_address : Network_address<IPV4_ADDR_LEN, '.', false>
{
	Ipv4_address(Genode::uint8_t value = 0) : Network_address(value) { }

	Ipv4_address(void *src) : Network_address(src) { }

	bool valid() const { return *this != Ipv4_address(); }
}
__attribute__((packed));


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
		Genode::uint8_t  protocol() const  { return _protocol;             }

		Genode::uint16_t checksum() { return host_to_big_endian(_header_checksum); }

		Ipv4_address dst() const { return Ipv4_address((void *)&_dst_addr); }
		Ipv4_address src() const { return Ipv4_address((void *)&_src_addr); }

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
		void * operator new(__SIZE_TYPE__ size, void* addr) { return addr; }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));


struct Net::Ipv4_address_prefix
{
	Ipv4_address    address;
	Genode::uint8_t prefix = 32;

	bool valid() const { return address.valid() || !prefix; }

	void print(Genode::Output &output) const;

	bool prefix_matches(Ipv4_address const &ip) const;
};


namespace Genode {

	inline size_t ascii_to(char const *s, Net::Ipv4_address &result);

	inline size_t ascii_to(char const *s, Net::Ipv4_address_prefix &result);
}


Genode::size_t Genode::ascii_to(char const *s, Net::Ipv4_address &result)
{
	Net::Ipv4_address buf;
	size_t            number_idx = 0;
	size_t            read_len   = 0;
	while (1) {

		/* read the current number, fail if there's no number */
		size_t number_len = ascii_to_unsigned(s, buf.addr[number_idx], 10);
		if (!number_len) {
			return 0; }

		/* update read length and number index */
		read_len += number_len;
		number_idx++;

		/* if we have all numbers, fill result and return read length */
		if (number_idx == sizeof(buf.addr) / sizeof(buf.addr[0])) {
			result = buf;
			return read_len;
		}
		/* as it was not the last number, check for the following dot */
		s += number_len;
		if (*s != '.') {
			return 0; }
		read_len++;
		s++;
	}
}


Genode::size_t Genode::ascii_to(char const *s, Net::Ipv4_address_prefix &result)
{
	/* read the leading IPv4 address, fail if there's no address */
	Net::Ipv4_address_prefix buf;
	size_t read_len = ascii_to(s, buf.address);
	if (!read_len) {
		return 0; }

	/* check for the following slash */
	s += read_len;
	if (*s != '/') {
		return 0; }
	read_len++;
	s++;

	/* read the prefix, fail if there's no prefix */
	size_t prefix_len = ascii_to_unsigned(s, buf.prefix, 10);
	if (!prefix_len) {
		return 0; }

	/* fill result and return read length */
	result = buf;
	return read_len + prefix_len;
}

#endif /* _IPV4_H_ */
