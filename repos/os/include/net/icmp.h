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
			DST_HOST_UNREACHABLE = 1,
			DST_PROTOCOL_UNREACHABLE = 2,
			DST_PORT_UNREACHABLE = 3,
			FRAGM_REQUIRED_AND_DF_FLAG_SET = 4,
			SOURCE_ROUTE_FAILED = 5,
			DST_NET_UNKNOWN = 6,
			DST_HOST_UNKNOWN = 7,
			SOURCE_HOST_ISOLATED = 8,
			NET_ADMINISTRATIVELY_PROHIB = 9,
			HOST_ADMINISTRATIVELY_PROHIB = 10,
			NET_UNREACHABLE_FOR_TOS = 11,
			HOST_UNREACHABLE_FOR_TOS = 12,
			COM_ADMINISTRATIVELY_PROHIB = 13,
			HOST_PRECEDENCE_VIOLATION = 14,
			PRECEDENCE_CUTOFF_IN_EFFECT = 15,
			ECHO_REQUEST = 0,
			ECHO_REPLY = 0,
			INVALID = 255,
		};

		static Code code_from_uint8(Type            type,
		                            Genode::uint8_t code)
		{
			switch (type) {
			case Type::DST_UNREACHABLE:
				switch (code) {
				case 0:  return Code::DST_NET_UNREACHABLE;
				case 1:  return Code::DST_HOST_UNREACHABLE;
				case 2:  return Code::DST_PROTOCOL_UNREACHABLE;
				case 3:  return Code::DST_PORT_UNREACHABLE;
				case 4:  return Code::FRAGM_REQUIRED_AND_DF_FLAG_SET;
				case 5:  return Code::SOURCE_ROUTE_FAILED;
				case 6:  return Code::DST_NET_UNKNOWN;
				case 7:  return Code::DST_HOST_UNKNOWN;
				case 8:  return Code::SOURCE_HOST_ISOLATED;
				case 9:  return Code::NET_ADMINISTRATIVELY_PROHIB;
				case 10: return Code::HOST_ADMINISTRATIVELY_PROHIB;
				case 11: return Code::NET_UNREACHABLE_FOR_TOS;
				case 12: return Code::HOST_UNREACHABLE_FOR_TOS;
				case 13: return Code::COM_ADMINISTRATIVELY_PROHIB;
				case 14: return Code::HOST_PRECEDENCE_VIOLATION;
				case 15: return Code::PRECEDENCE_CUTOFF_IN_EFFECT;
				default: break;
				}
				break;
			case Type::ECHO_REPLY:
				switch (code) {
				case 0:  return Code::ECHO_REPLY;
				default: break;
				}
				break;
			case Type::ECHO_REQUEST:
				switch (code) {
				case 0:  return Code::ECHO_REQUEST;
				default: break;
				}
				break;
			}
			return Code::INVALID;
		}

		void update_checksum(Genode::size_t data_sz);

		void update_checksum(Internet_checksum_diff const &icd);

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

		void type_and_code(Type t, Code c, Internet_checksum_diff &icd);
		void query_id(Genode::uint16_t v, Internet_checksum_diff &icd);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _NET__ICMP_H_ */
