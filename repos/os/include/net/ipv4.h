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
#include <util/register.h>
#include <net/netaddress.h>
#include <net/size_guard.h>

namespace Genode { class Output; }

namespace Net {

	enum { IPV4_ADDR_LEN = 4 };

	class Internet_checksum_diff;
	class Ipv4_address;
	class Ipv4_packet;

	static inline Genode::size_t ascii_to(char const *, Net::Ipv4_address &);
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

		void update_checksum(Internet_checksum_diff const &icd);

		bool checksum_error() const;

	private:

		/************************
		 ** IPv4 header fields **
		 ************************/

		struct Offset_6_u16 : Genode::Register<16>
		{
			struct Fragment_offset : Bitfield<0, 13> { };
			struct Flags           : Bitfield<13, 3>  { };
			struct More_fragments  : Bitfield<13, 1>  { };
			struct Dont_fragment   : Bitfield<14, 1>  { };
		};

		struct Offset_0_u8 : Genode::Register<8>
		{
			struct Ihl     : Bitfield<0, 4> { }; /* Internet Header Length */
			struct Version : Bitfield<4, 4> { };
		};

		struct Offset_1_u8 : Genode::Register<8>
		{
			struct Ecn  : Bitfield<0, 2> { }; /* Explicit Congestion Notification */
			struct Dscp : Bitfield<2, 6> { }; /* Differentiated Services Code Point */
		};

		Genode::uint8_t  _offset_0_u8;
		Genode::uint8_t  _offset_1_u8;
		Genode::uint16_t _total_length;
		Genode::uint16_t _identification;
		Genode::uint16_t _offset_6_u16;
		Genode::uint8_t  _time_to_live;
		Genode::uint8_t  _protocol;
		Genode::uint16_t _checksum;
		Genode::uint8_t  _src[ADDR_LEN];
		Genode::uint8_t  _dst[ADDR_LEN];
		unsigned _data[0];

	public:

		enum class Protocol : Genode::uint8_t
		{
			ICMP = 1,
			TCP  = 6,
			UDP  = 17,
		};

		static Ipv4_packet const &cast_from(void const *base,
		                                    Size_guard &size_guard)
		{
			size_guard.consume_head(sizeof(Ipv4_packet));
			return *(Ipv4_packet const *)base;
		}

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

		Genode::size_t       header_length()   const { return Offset_0_u8::Ihl::get(_offset_0_u8); }
		Genode::uint8_t      version()         const { return Offset_0_u8::Version::get(_offset_0_u8); }
		Genode::uint8_t      diff_service()    const { return Offset_1_u8::Dscp::get(_offset_1_u8); }
		Genode::uint8_t      ecn()             const { return Offset_1_u8::Ecn::get(_offset_1_u8); }
		Genode::size_t       total_length()    const { return host_to_big_endian(_total_length); }
		Genode::uint16_t     identification()  const { return host_to_big_endian(_identification); }
		Genode::uint8_t      flags()           const { return (Genode::uint8_t)Offset_6_u16::Flags::get(host_to_big_endian(_offset_6_u16)); }
		bool                 dont_fragment()   const { return Offset_6_u16::Dont_fragment::get(host_to_big_endian(_offset_6_u16)); }
		bool                 more_fragments()  const { return Offset_6_u16::More_fragments::get(host_to_big_endian(_offset_6_u16)); }
		Genode::size_t       fragment_offset() const { return Offset_6_u16::Fragment_offset::get(host_to_big_endian(_offset_6_u16)); }
		Genode::uint8_t      time_to_live()    const { return _time_to_live; }
		Protocol             protocol()        const { return (Protocol)_protocol; }
		Genode::uint16_t     checksum()        const { return host_to_big_endian(_checksum); }
		Ipv4_address         src()             const { return Ipv4_address((void *)&_src); }
		Ipv4_address         dst()             const { return Ipv4_address((void *)&_dst); }

		void header_length(Genode::size_t v)     { Offset_0_u8::Ihl::set(_offset_0_u8, (Offset_0_u8::access_t)v); }
		void version(Genode::uint8_t v)          { Offset_0_u8::Version::set(_offset_0_u8, v); }
		void diff_service(Genode::uint8_t v)     { Offset_1_u8::Dscp::set(_offset_1_u8, v); }
		void ecn(Genode::uint8_t v)              { Offset_1_u8::Ecn::set(_offset_1_u8, v); }
		void diff_service_ecn(Genode::uint8_t v) { _offset_1_u8 = v; }
		void total_length(Genode::size_t v)      { _total_length = host_to_big_endian((Genode::uint16_t)v); }
		void identification(Genode::uint16_t v)  { _identification = host_to_big_endian(v); }
		void time_to_live(Genode::uint8_t v)     { _time_to_live = v; }
		void protocol(Protocol v)                { _protocol = (Genode::uint8_t)v; }
		void checksum(Genode::uint16_t checksum) { _checksum = host_to_big_endian(checksum); }
		void src(Ipv4_address v)                 { v.copy(&_src); }
		void dst(Ipv4_address v)                 { v.copy(&_dst); }
		void src_big_endian(Genode::uint32_t v)  { *(Genode::uint32_t *)&_src = v; }
		void dst_big_endian(Genode::uint32_t v)  { *(Genode::uint32_t *)&_dst = v; }

		void flags(Genode::uint8_t v)
		{
			Genode::uint16_t be = host_to_big_endian(_offset_6_u16);
			Offset_6_u16::Flags::set(be, v);
			_offset_6_u16 = host_to_big_endian(be);
		}

		void fragment_offset(Genode::size_t v)
		{
			Genode::uint16_t be = host_to_big_endian(_offset_6_u16);
			Offset_6_u16::Fragment_offset::set(be, (Offset_6_u16::access_t)v);
			_offset_6_u16 = host_to_big_endian(be);
		}

		void dont_fragment(bool v)
		{
			Genode::uint16_t be = host_to_big_endian(_offset_6_u16);
			Offset_6_u16::Dont_fragment::set(be, v);
			_offset_6_u16 = host_to_big_endian(be);
		}

		void more_fragments(bool v)
		{
			Genode::uint16_t be = host_to_big_endian(_offset_6_u16);
			Offset_6_u16::More_fragments::set(be, v);
			_offset_6_u16 = host_to_big_endian(be);
		}

		void src(Ipv4_address v, Internet_checksum_diff &icd);
		void dst(Ipv4_address v, Internet_checksum_diff &icd);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));


Genode::size_t Net::ascii_to(char const *s, Net::Ipv4_address &result)
{
	using namespace Genode;

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
