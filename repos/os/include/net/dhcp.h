/*
 * \brief  DHCP related definitions
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DHCP_H_
#define _DHCP_H_

/* Genode */
#include <base/exception.h>
#include <base/stdint.h>

#include <util/endian.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>

namespace Net { class Dhcp_packet; }


/**
 * Data layout of this class conforms to an DHCP packet (RFC 2131)
 *
 * DHCP packet layout:
 *
 *  ===================================
 * | 1 byte | 1 byte | 1 byte | 1 byte |
 *  ===================================
 * |   op   |  htype |  hlen  |  hops  |
 *  -----------------------------------
 * |       connection-id (xid)         |
 *  -----------------------------------
 * |    seconds      |      flags      |
 *  -----------------------------------
 * |         client-ip-address         |
 *  -----------------------------------
 * |           your-ip-address         |
 *  -----------------------------------
 * |         server-ip-address         |
 *  -----------------------------------
 * |       relay-agent-ip-address      |
 *  -----------------------------------
 * |          client-hw-address        |
 * |             (16 bytes)            |
 *  -----------------------------------
 * |              sname                |
 * |            (64 bytes)             |
 *  -----------------------------------
 * |               file                |
 * |            (128 bytes)            |
 *  -----------------------------------
 * |              options              |
 * |      (312 bytes, optional)        |
 *  -----------------------------------
 */
class Net::Dhcp_packet
{
	public:

		class No_dhcp_packet : Genode::Exception {};


		class Client_hw_address
		{
			public:
				Genode::uint8_t addr[16];
		};

	private:

		/************************
		 ** DHCP packet fields **
		 ************************/

		Genode::uint8_t   _op;
		Genode::uint8_t   _htype;
		Genode::uint8_t   _hlen;
		Genode::uint8_t   _hops;
		Genode::uint32_t  _xid;
		Genode::uint16_t  _secs;
		Genode::uint16_t  _flags;
		Genode::uint8_t   _ciaddr[Ipv4_packet::ADDR_LEN];
		Genode::uint8_t   _yiaddr[Ipv4_packet::ADDR_LEN];
		Genode::uint8_t   _siaddr[Ipv4_packet::ADDR_LEN];
		Genode::uint8_t   _giaddr[Ipv4_packet::ADDR_LEN];
		Client_hw_address _chaddr;
		Genode::uint8_t   _sname[64];
		Genode::uint8_t   _file[128];
		Genode::uint32_t  _magic_cookie;
		Genode::uint8_t   _opts[0];

		enum Flag {
			BROADCAST = 0x80
		};

	public:

		/**
		 * This class represents the data layout of an DHCP option.
		 */
		class Option
		{
			private:

				Genode::uint8_t _code;
				Genode::uint8_t _len;
				Genode::uint8_t _value[0];

			public:

				Option() {}

				Genode::uint8_t code()   { return _code;  }
				Genode::size_t  length() { return _len;   }
				void*           value()  { return _value; }

				/**
				 * Placement new.
				 */
				void * operator new(Genode::size_t size, void* addr) {
					return addr; }
		} __attribute__((packed));


		enum Opcode {
			REQUEST = 1,
			REPLY   = 2,
			INVALID
		};

		enum Udp_port {
			BOOTPS = 67,
			BOOTPC = 68
		};

		enum Option_type {
			REQ_IP_ADDR    = 50,
			IP_LEASE_TIME  = 51,
			OPT_OVERLOAD   = 52,
			MSG_TYPE       = 53,
			SRV_ID         = 54,
			REQ_PARAMETER  = 55,
			MESSAGE        = 56,
			MAX_MSG_SZ     = 57,
			RENEWAL        = 58,
			REBINDING      = 59,
			VENDOR         = 60,
			CLI_ID         = 61,
			TFTP_SRV_NAME  = 66,
			BOOT_FILE      = 67,
			END            = 255
		};

		enum Message_type {
			DHCP_DISCOVER  = 1,
			DHCP_OFFER     = 2,
			DHCP_REQUEST   = 3,
			DHCP_DECLINE   = 4,
			DHCP_ACK       = 5,
			DHCP_NAK       = 6,
			DHCP_RELEASE   = 7,
			DHCP_INFORM    = 8
		};


		/*****************
		 ** Constructor **
		 *****************/

		Dhcp_packet(Genode::size_t size) {
			/* dhcp packet needs to fit in */
			if (size < sizeof(Dhcp_packet))
				throw No_dhcp_packet();
		}


		/*******************************
		 ** DHCP field read-accessors **
		 *******************************/

		Genode::uint8_t   op()    { return _op;          }
		Genode::uint8_t   htype() { return _htype;       }
		Genode::uint8_t   hlen()  { return _hlen;        }
		Genode::uint8_t   hops()  { return _hops;        }
		Genode::uint32_t  xid()   { return host_to_big_endian(_xid);  }
		Genode::uint16_t  secs()  { return host_to_big_endian(_secs); }

		bool broadcast() { return _flags & BROADCAST;    }

		Ipv4_address ciaddr() {
			return Ipv4_address(&_ciaddr);  }
		Ipv4_address yiaddr() {
			return Ipv4_address(&_yiaddr);  }
		Ipv4_address siaddr() {
			return Ipv4_address(&_siaddr);  }
		Ipv4_address giaddr() {
			return Ipv4_address(&_giaddr);  }

		Mac_address client_mac() {
			return Mac_address(&_chaddr); }

		const char* server_name()  { return (const char*) &_sname; }
		const char* file()         { return (const char*) &_file;  }

		Option *option(Option_type op)
		{
			void *ptr = &_opts;
			while (true) {
				Option *ext = new (ptr) Option();
				if (ext->code() == op)
					return ext;
				if (ext->code() == END || ext->code() == 0)
					break;
				ptr = ext + ext->length();
			}
			return 0;
		}


		/*******************************
		 ** DHCP field write-accessors **
		 *******************************/

		void op(Genode::uint8_t op)       { _op = op;             }
		void htype(Genode::uint8_t htype) { _htype = htype;       }
		void hlen(Genode::uint8_t hlen)   { _hlen = hlen;         }
		void hops(Genode::uint8_t hops)   { _hops = hops;         }
		void xid(Genode::uint32_t xid)    { _xid = host_to_big_endian(_xid);   }
		void secs(Genode::uint16_t secs)  { _secs = host_to_big_endian(_secs); }

		void broadcast(bool broadcast) {
			_flags = broadcast ? BROADCAST : 0;       }

		void ciaddr(Ipv4_address ciaddr) {
			ciaddr.copy(&_ciaddr); }
		void yiaddr(Ipv4_address yiaddr) {
			yiaddr.copy(&_yiaddr); }
		void siaddr(Ipv4_address siaddr) {
			siaddr.copy(&_siaddr); }
		void giaddr(Ipv4_address giaddr) {
			giaddr.copy(&_giaddr); }

		void client_mac(Mac_address mac) {
			mac.copy(&_chaddr); }


		/*************************
		 ** Convenience methods **
		 *************************/

		static bool is_dhcp(Udp_packet *udp)
		{
			return ((udp->src_port() == Dhcp_packet::BOOTPC ||
			         udp->src_port() == Dhcp_packet::BOOTPS) &&
			        (udp->dst_port() == Dhcp_packet::BOOTPC ||
			         udp->dst_port() == Dhcp_packet::BOOTPS));
		}


		/***************
		 ** Operators **
		 ***************/

		/**
		 * Placement new.
		 */
		void * operator new(Genode::size_t size, void* addr) {
			return addr; }

} __attribute__((packed));

#endif /* _DHCP_H_ */
