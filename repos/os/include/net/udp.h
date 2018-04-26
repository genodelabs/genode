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

		void update_checksum(Ipv4_address ip_src,
		                     Ipv4_address ip_dst);

		bool checksum_error(Ipv4_address ip_src,
		                    Ipv4_address ip_dst) const;


		/***************
		 ** Accessors **
		 ***************/

		Port             src_port() const { return Port(host_to_big_endian(_src_port)); }
		Port             dst_port() const { return Port(host_to_big_endian(_dst_port)); }
		Genode::uint16_t length()   const { return host_to_big_endian(_length);   }
		Genode::uint16_t checksum() const { return host_to_big_endian(_checksum); }

		void length(Genode::uint16_t v) { _length = host_to_big_endian(v); }
		void src_port(Port p)           { _src_port = host_to_big_endian(p.value); }
		void dst_port(Port p)           { _dst_port = host_to_big_endian(p.value); }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _UDP_H_ */
