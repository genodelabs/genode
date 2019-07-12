/*
 * \brief  Simple Network Time Protocol (SNTP) Version 4 (RFC 4330)
 * \author Martin Stein
 * \date   2019-07-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__SNTP_H_
#define _NET__SNTP_H_

/* Genode includes */
#include <util/register.h>

namespace Net
{
	class Sntp_timestamp;
	class Sntp_packet;
}


class Net::Sntp_timestamp
{
	private:

		using uint32_t = Genode::uint32_t;
		using uint64_t = Genode::uint64_t;

		enum { UNIX_TS_OFFSET_SEC = 2208988800 };

		uint32_t const _seconds;
		uint32_t const _seconds_fraction;

	public:

		Sntp_timestamp(uint64_t const plain_value)
		:
			_seconds          { (uint32_t)(plain_value >> 32) },
			_seconds_fraction { (uint32_t)(plain_value) }
		{ }

		uint64_t to_unix_timestamp() const
		{
			return _seconds - UNIX_TS_OFFSET_SEC;
		}

		static Sntp_timestamp from_unix_timestamp(uint64_t const unix_ts)
		{
			return unix_ts + UNIX_TS_OFFSET_SEC;
		}


		/***************
		 ** Accessors **
		 ***************/

		uint32_t seconds()          const { return _seconds; }
		uint32_t seconds_fraction() const { return _seconds_fraction; }
};


class Net::Sntp_packet
{
	private:

		Genode::uint8_t  _byte_0;
		Genode::uint8_t  _stratum;
		Genode::uint8_t  _poll;
		Genode::uint8_t  _precision;
		Genode::uint32_t _root_delay;
		Genode::uint32_t _root_dispersion;
		Genode::uint32_t _reference_identifier;
		Genode::uint64_t _reference_timestamp;
		Genode::uint64_t _originate_timestamp;
		Genode::uint64_t _receive_timestamp;
		Genode::uint64_t _transmit_timestamp;

		struct Byte_0 : Genode::Register<8> {
			struct Mode           : Bitfield<0, 3> { };
			struct Version_number : Bitfield<3, 3> { };
		};

	public:

		enum { UDP_PORT       = 123 };
		enum { VERSION_NUMBER = 4   };
		enum { MODE_CLIENT    = 3   };
		enum { MODE_SERVER    = 4   };


		/***************
		 ** Accessors **
		 ***************/

		void version_number(Genode::uint8_t v) { Byte_0::Version_number::set(_byte_0, v); }
		void mode          (Genode::uint8_t v) { Byte_0::Mode          ::set(_byte_0, v); }

		Byte_0::access_t version_number()      const { return Byte_0::Version_number::get(_byte_0); }
		Byte_0::access_t mode()                const { return Byte_0::Mode::get(_byte_0); }
		Genode::uint64_t transmit_timestamp()  const { return host_to_big_endian(_transmit_timestamp); }
		Genode::uint64_t receive_timestamp()   const { return host_to_big_endian(_receive_timestamp); }
		Genode::uint64_t originate_timestamp() const { return host_to_big_endian(_originate_timestamp); }

} __attribute__((packed));

#endif /* _NET__SNTP_H_ */
