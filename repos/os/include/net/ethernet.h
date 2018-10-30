/*
 * \brief  Ethernet protocol
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__ETHERNET_H_
#define _NET__ETHERNET_H_

/* Genode includes */
#include <base/exception.h>
#include <util/string.h>
#include <util/construct_at.h>
#include <util/endian.h>
#include <net/mac_address.h>
#include <net/size_guard.h>

namespace Net
{
	class Ethernet_frame;

	template <Genode::size_t DATA_SIZE>
	class Ethernet_frame_sized;
}


/**
 * Data layout of this class conforms to the Ethernet II frame
 * (IEEE 802.3).
 *
 * Ethernet-frame-header-format:
 *
 *  ----------------------------------------------------------
 * | destination mac address | source mac address | ethertype |
 * |      6 bytes            |     6 bytes        |  2 bytes  |
 *  ----------------------------------------------------------
 */
class Net::Ethernet_frame
{
	public:

		enum Size {
			ADDR_LEN  = 6, /* MAC address length in bytes */
		};

		static Mac_address broadcast() { return Mac_address((Genode::uint8_t)0xff); }

	private:

		Genode::uint8_t  _dst[ADDR_LEN]; /* destination mac address */
		Genode::uint8_t  _src[ADDR_LEN]; /* source mac address */
		Genode::uint16_t _type;          /* encapsulated protocol */
		unsigned         _data[0];       /* encapsulated data */

	public:

		enum { MIN_SIZE = 64 };

		/**
		 * Id representing encapsulated protocol.
		 */
		enum class Type : Genode::uint16_t {
			IPV4 = 0x0800,
			ARP  = 0x0806,
		};

		template <typename T>
		T const &data(Size_guard &size_guard) const
		{
			size_guard.consume_head(sizeof(T));
			T const &obj = *(T *)(_data);

			/* Ethernet may have a tail whose size must be considered */
			Genode::size_t const unconsumed = size_guard.unconsumed();
			size_guard.consume_tail(unconsumed + sizeof(T) -
			                        obj.size(unconsumed));
			return obj;
		}

		template <typename T>
		T &data(Size_guard &size_guard)
		{
			size_guard.consume_head(sizeof(T));
			T &obj = *(T *)(_data);

			/* Ethernet may have a tail whose size must be considered */
			Genode::size_t const max_obj_sz = size_guard.unconsumed() + sizeof(T);
			size_guard.consume_tail(max_obj_sz - obj.size(max_obj_sz));
			return obj;
		}

		template <typename T>
		T &construct_at_data(Size_guard &size_guard)
		{
			size_guard.consume_head(sizeof(T));
			return *Genode::construct_at<T>(_data);
		}

		static Ethernet_frame &construct_at(void       *base,
		                                    Size_guard &size_guard)
		{
			size_guard.consume_head(sizeof(Ethernet_frame));
			return *Genode::construct_at<Ethernet_frame>(base);
		}

		static Ethernet_frame &cast_from(void       *base,
		                                 Size_guard &size_guard)
		{
			size_guard.consume_head(sizeof(Ethernet_frame));
			return *(Ethernet_frame *)base;
		}


		/***************
		 ** Accessors **
		 ***************/

		Mac_address dst()  const { return Mac_address((void *)_dst); }
		Mac_address src()  const { return Mac_address((void *)_src); }
		Type        type() const { return (Type)host_to_big_endian(_type); }

		void dst(Mac_address v) { v.copy(&_dst); }
		void src(Mac_address v) { v.copy(&_src); }
		void type(Type type)    { _type = host_to_big_endian((Genode::uint16_t)type); }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));


template <Genode::size_t DATA_SIZE>
class Net::Ethernet_frame_sized : public Ethernet_frame
{
	private:

		enum {
			HS = sizeof(Ethernet_frame),
			DS = DATA_SIZE + HS >= MIN_SIZE ? DATA_SIZE : MIN_SIZE - HS,
		};

		Genode::uint8_t  _data[DS];
		Genode::uint32_t _checksum;

	public:

		Ethernet_frame_sized(Mac_address dst_in, Mac_address src_in,
		                     Type type_in)
		:
			Ethernet_frame()
		{
			dst(dst_in);
			src(src_in);
			type(type_in);
		}

} __attribute__((packed));

#endif /* _NET__ETHERNET_H_ */
