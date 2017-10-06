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

#include <util/endian.h>
#include <net/mac_address.h>

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


		static const Mac_address BROADCAST;  /* broadcast address */

	private:

		Genode::uint8_t  _dst[ADDR_LEN]; /* destination mac address */
		Genode::uint8_t  _src[ADDR_LEN]; /* source mac address */
		Genode::uint16_t _type;          /* encapsulated protocol */
		unsigned         _data[0];       /* encapsulated data */

	public:

		class No_ethernet_frame : Genode::Exception {};

		enum { MIN_SIZE = 64 };

		/**
		 * Id representing encapsulated protocol.
		 */
		enum class Type : Genode::uint16_t {
			IPV4 = 0x0800,
			ARP  = 0x0806,
		};


		/*****************
		 ** Constructor **
		 *****************/

		Ethernet_frame(Genode::size_t size) {
			/* at least, frame header needs to fit in */
			if (size < sizeof(Ethernet_frame))
				throw No_ethernet_frame();
		}

		/**
		 * Constructor for composing a new Ethernet frame
		 */
		Ethernet_frame() { }


		/***************
		 ** Accessors **
		 ***************/

		Mac_address                    dst()  const { return Mac_address((void *)_dst); }
		Mac_address                    src()  const { return Mac_address((void *)_src); }
		Type                           type() const { return (Type)host_to_big_endian(_type); }
		template <typename T> T const *data() const { return (T const *)(_data); }
		template <typename T> T       *data()       { return (T *)(_data); }

		void dst(Mac_address v) { v.copy(&_dst); }
		void src(Mac_address v) { v.copy(&_src); }
		void type(Type type)    { _type = host_to_big_endian((Genode::uint16_t)type); }


		/***************
		 ** Operators **
		 ***************/

		/**
		 * Placement new operator.
		 */
		void * operator new(__SIZE_TYPE__ size, void* addr) { return addr; }


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
			Ethernet_frame(sizeof(Ethernet_frame))
		{
			dst(dst_in);
			src(src_in);
			type(type_in);
		}

} __attribute__((packed));

#endif /* _NET__ETHERNET_H_ */
