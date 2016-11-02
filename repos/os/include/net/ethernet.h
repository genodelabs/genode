/*
 * \brief  Ethernet protocol
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

		Genode::uint8_t  _dst_mac[ADDR_LEN]; /* destination mac address */
		Genode::uint8_t  _src_mac[ADDR_LEN]; /* source mac address */
		Genode::uint16_t _type;              /* encapsulated protocol */
		unsigned         _data[0];           /* encapsulated data */

	public:

		class No_ethernet_frame : Genode::Exception {};

		enum { MIN_SIZE = 64 };

		/**
		 * Id representing encapsulated protocol.
		 */
		enum Ether_type {
			IPV4              = 0x0800,
			ARP               = 0x0806,
			WAKE_ON_LAN       = 0x0842,
			SYN3              = 0x1337,
			RARP              = 0x8035,
			APPLETALK         = 0x809B,
			AARP              = 0x80F3,
			VLAN_TAGGED       = 0x8100,
			IPX               = 0x8137,
			NOVELL            = 0x8138,
			IPV6              = 0x86DD,
			MAC_CONTROL       = 0x8808,
			SLOW              = 0x8809,
			COBRANET          = 0x8819,
			MPLS_UNICAST      = 0x8847,
			MPLS_MULTICAST    = 0x8848,
			PPPOE_DISCOVERY   = 0x8863,
			PPPOE_STAGE       = 0x8864,
			NLB               = 0x886F,
			JUMBO_FRAMES      = 0x8870,
			EAP               = 0x888E,
			PROFINET          = 0x8892,
			HYPERSCSI         = 0x889A,
			ATAOE             = 0x88A2,
			ETHERCAT          = 0x88A4,
			PROVIDER_BRIDGING = 0x88A8,
			POWERLINK         = 0x88AB,
			LLDP              = 0x88CC,
			SERCOS_III        = 0x88CD,
			CESOE             = 0x88D8,
			HOMEPLUG          = 0x88E1,
			MAC_SEC           = 0x88E5,
			PRECISION_TIME    = 0x88F7,
			CFM               = 0x8902,
			FCOE              = 0x8906,
			FCOE_Init         = 0x8914,
			Q_IN_Q            = 0x9100,
			LLT               = 0xCAFE
		};


		/*****************
		 ** Constructor **
		 *****************/

		Ethernet_frame(Genode::size_t size) {
			/* at least, frame header needs to fit in */
			if (size < sizeof(Ethernet_frame))
				throw No_ethernet_frame();
		}


		/***********************************
		 ** Ethernet field read-accessors **
		 ***********************************/

		/**
		 * \return destination MAC address of frame.
		 */
		Mac_address dst() const { return Mac_address((void *)&_dst_mac); }

		/**
		 * \return source MAC address of frame.
		 */
		Mac_address src() const { return Mac_address((void *)&_src_mac); }

		/**
		 * \return EtherType - type of encapsulated protocol.
		 */
		Genode::uint16_t type() const { return host_to_big_endian(_type); }

		/**
		 * \return payload data.
		 */
		template <typename T> T *       data()       { return (T *)(_data); }
		template <typename T> T const * data() const { return (T const *)(_data); }


		/***********************************
		 ** Ethernet field write-accessors **
		 ***********************************/

		/**
		 * Set the destination MAC address of this frame.
		 *
		 * \param mac  MAC address to be set.
		 */
		void dst(Mac_address mac) { mac.copy(&_dst_mac); }

		/**
		 * Set the source MAC address of this frame.
		 *
		 * \param mac  MAC address to be set.
		 */
		void src(Mac_address mac) { mac.copy(&_src_mac); }

		/**
		 * Set type of encapsulated protocol.
		 *
		 * \param type  the EtherType to be set.
		 */
		void type(Genode::uint16_t type) { _type = host_to_big_endian(type); }


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
		                     Ether_type type_in)
		:
			Ethernet_frame(sizeof(Ethernet_frame))
		{
			dst(dst_in);
			src(src_in);
			type(type_in);
		}

} __attribute__((packed));

#endif /* _NET__ETHERNET_H_ */
