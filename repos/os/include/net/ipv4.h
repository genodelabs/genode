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
#include <util/construct_at.h>
#include <util/endian.h>
#include <net/netaddress.h>
#include <net/size_guard.h>

namespace Genode { class Output; }

namespace Net
{
	enum { IPV4_ADDR_LEN = 4 };

	class Ipv4_address;

	class Ipv4_packet;
}


struct Net::Ipv4_address : Network_address<IPV4_ADDR_LEN, '.', false>
{
	Ipv4_address(Genode::uint8_t value = 0) : Network_address(value) { }

	Ipv4_address(void *src) : Network_address(src) { }

	bool valid() const { return *this != Ipv4_address(); }

	Genode::uint32_t to_uint32_big_endian() const;

	static Ipv4_address from_uint32_big_endian(Genode::uint32_t ip_raw);

	Genode::uint32_t to_uint32_little_endian() const;

	static Ipv4_address from_uint32_little_endian(Genode::uint32_t ip_raw);

	bool is_in_range(Ipv4_address const &first,
	                 Ipv4_address const &last) const;
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

		static Ipv4_address current()   { return Ipv4_address((Genode::uint8_t)0x00); }
		static Ipv4_address broadcast() { return Ipv4_address((Genode::uint8_t)0xff); }

		static Ipv4_address ip_from_string(const char *ip);

		void update_checksum();

		bool checksum_error() const;

	private:

		/************************
		 ** IPv4 header fields **
		 ************************/

		unsigned         _header_length   : 4;
		unsigned         _version         : 4;
		unsigned         _diff_service    : 6;
		unsigned         _ecn             : 2;
		Genode::uint16_t _total_length;
		Genode::uint16_t _identification;
		unsigned         _flags           : 3;
		unsigned         _fragment_offset : 13;
		Genode::uint8_t  _time_to_live;
		Genode::uint8_t  _protocol;
		Genode::uint16_t _checksum;
		Genode::uint8_t  _src[ADDR_LEN];
		Genode::uint8_t  _dst[ADDR_LEN];
		unsigned         _data[0];

	public:

		enum class Protocol : Genode::uint8_t
		{
			ICMP = 1,
			TCP  = 6,
			UDP  = 17,
		};

		template <typename T>
		T const &data(Size_guard &size_guard) const
		{
			size_guard.consume_head(sizeof(T));
			return *(T const *)(_data);
		}

		template <typename T>
		T &data(Size_guard &size_guard)
		{
			size_guard.consume_head(sizeof(T));
			return *(T *)(_data);
		}

		template <typename T, typename SIZE_GUARD>
		T &construct_at_data(SIZE_GUARD &size_guard)
		{
			size_guard.consume_head(sizeof(T));
			return *Genode::construct_at<T>(_data);
		}

		Genode::size_t size(Genode::size_t max_size) const;


		/***************
		 ** Accessors **
		 ***************/

		Genode::size_t   header_length()   const { return _header_length; }
		Genode::uint8_t  version()         const { return _version; }
		Genode::uint8_t  diff_service()    const { return _diff_service; }
		Genode::uint8_t  ecn()             const { return _ecn; }
		Genode::size_t   total_length()    const { return host_to_big_endian(_total_length); }
		Genode::uint16_t identification()  const { return host_to_big_endian(_identification); }
		Genode::uint8_t  flags()           const { return _flags; }
		Genode::size_t   fragment_offset() const { return _fragment_offset; }
		Genode::uint8_t  time_to_live()    const { return _time_to_live; }
		Protocol         protocol()        const { return (Protocol)_protocol; }
		Genode::uint16_t checksum()        const { return host_to_big_endian(_checksum); }
		Ipv4_address     src()             const { return Ipv4_address((void *)&_src); }
		Ipv4_address     dst()             const { return Ipv4_address((void *)&_dst); }

		void header_length(Genode::size_t v)     { _header_length = v; }
		void version(Genode::uint8_t v)          { _version = v; }
		void diff_service(Genode::uint8_t v)     { _diff_service = v; ; }
		void ecn(Genode::uint8_t v)              { _ecn = v; ; }
		void total_length(Genode::size_t v)      { _total_length = host_to_big_endian((Genode::uint16_t)v); }
		void identification(Genode::uint16_t v)  { _identification = host_to_big_endian(v); }
		void flags(Genode::uint8_t v)            { _flags = v; ; }
		void fragment_offset(Genode::size_t v)   { _fragment_offset = v; ; }
		void time_to_live(Genode::uint8_t v)     { _time_to_live = v; }
		void protocol(Protocol v)                { _protocol = (Genode::uint8_t)v; }
		void checksum(Genode::uint16_t checksum) { _checksum = host_to_big_endian(checksum); }
		void src(Ipv4_address v)                 { v.copy(&_src); }
		void dst(Ipv4_address v)                 { v.copy(&_dst); }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));


namespace Genode {

	inline size_t ascii_to(char const *s, Net::Ipv4_address &result);
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

#endif /* _IPV4_H_ */
