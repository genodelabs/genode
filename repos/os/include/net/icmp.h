/*
 * \brief  Internet Control Message Protocol
 * \author Martin Stein
 * \date   2018-03-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__ICMP_H_
#define _NET__ICMP_H_

/* Genode includes */
#include <util/endian.h>
#include <base/exception.h>

namespace Genode { class Output; }

namespace Net { class Icmp_packet; }


class Net::Icmp_packet
{
	private:

		Genode::uint8_t  _type;
		Genode::uint8_t  _code;
		Genode::uint16_t _checksum;
		union {
			Genode::uint32_t _rest_of_header_u32[1];
			Genode::uint16_t _rest_of_header_u16[2];
			Genode::uint8_t  _rest_of_header_u8[4];

		} __attribute__((packed));

		Genode::uint8_t _data[0];

	public:

		enum class Type
		{
			ECHO_REPLY      = 0,
			DST_UNREACHABLE = 3,
			ECHO_REQUEST    = 8,
		};

		enum class Code
		{
			DST_NET_UNREACHABLE = 0,
			ECHO_REQUEST        = 0,
			ECHO_REPLY          = 0,
		};

		void update_checksum(Genode::size_t data_sz);

		bool checksum_error(Genode::size_t data_sz) const;


		/***************
		 ** Accessors **
		 ***************/

		Type              type()           const { return (Type)_type; }
		Code              code()           const { return (Code)_code; }
		Genode::uint16_t  checksum()       const { return host_to_big_endian(_checksum); }
		Genode::uint16_t  query_id()       const { return host_to_big_endian(_rest_of_header_u16[0]); }
		Genode::uint16_t  query_seq()      const { return host_to_big_endian(_rest_of_header_u16[1]); }
		Genode::uint32_t  rest_of_header() const { return host_to_big_endian(_rest_of_header_u32[0]); }

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

		void copy_to_data(void     const *base,
		                  Genode::size_t  size,
		                  Size_guard     &size_guard)
		{
			size_guard.consume_head(size);
			Genode::memcpy(_data, base, size);
		}

		void type(Type v)                       { _type = (Genode::uint8_t)v; }
		void code(Code v)                       { _code = (Genode::uint8_t)v; }
		void checksum(Genode::uint16_t v)       { _checksum = host_to_big_endian(v); }
		void rest_of_header(Genode::uint32_t v) { _rest_of_header_u32[0] = host_to_big_endian(v); }
		void query_id(Genode::uint16_t v)       { _rest_of_header_u16[0] = host_to_big_endian(v); }
		void query_seq(Genode::uint16_t v)      { _rest_of_header_u16[1] = host_to_big_endian(v); }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _NET__ICMP_H_ */
