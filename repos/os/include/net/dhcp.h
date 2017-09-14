/*
 * \brief  DHCP related definitions
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

	private:

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
		Genode::uint8_t   _chaddr[16];
		Genode::uint8_t   _sname[64];
		Genode::uint8_t   _file[128];
		Genode::uint32_t  _magic_cookie;
		Genode::uint8_t   _opts[0];

		enum Flag { BROADCAST = 0x80 };

	public:

		enum class Htype : Genode::uint8_t { ETH = 1 };

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
				void * operator new(__SIZE_TYPE__, void* addr) { return addr; }
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


		/***************
		 ** Accessors **
		 ***************/

		Genode::uint8_t   op()           const { return _op; }
		Htype             htype()        const { return (Htype)_htype; }
		Genode::uint8_t   hlen()         const { return _hlen; }
		Genode::uint8_t   hops()         const { return _hops; }
		Genode::uint32_t  xid()          const { return host_to_big_endian(_xid); }
		Genode::uint16_t  secs()         const { return host_to_big_endian(_secs); }
		bool              broadcast()    const { return _flags & BROADCAST; }
		Ipv4_address      ciaddr()       const { return Ipv4_address((void *)_ciaddr);  }
		Ipv4_address      yiaddr()       const { return Ipv4_address((void *)_yiaddr);  }
		Ipv4_address      siaddr()       const { return Ipv4_address((void *)_siaddr);  }
		Ipv4_address      giaddr()       const { return Ipv4_address((void *)_giaddr);  }
		Mac_address       client_mac()   const { return Mac_address((void *)&_chaddr); }
		char const       *server_name()  const { return (char const *)&_sname; }
		char const       *file()         const { return (char const *)&_file;  }
		Genode::uint32_t  magic_cookie() const { return host_to_big_endian(_magic_cookie); }
		Genode::uint16_t  flags()        const { return host_to_big_endian(_flags); }
		void             *opts()         const { return (void *)_opts; }

		void flags(Genode::uint16_t v) { _flags = host_to_big_endian(v); }
		void file(const char* v)       { Genode::memcpy(_file, v, sizeof(_file));  }
		void op(Genode::uint8_t v)     { _op = v; }
		void htype(Htype v)            { _htype = (Genode::uint8_t)v; }
		void hlen(Genode::uint8_t v)   { _hlen = v; }
		void hops(Genode::uint8_t v)   { _hops = v; }
		void xid(Genode::uint32_t v)   { _xid  = host_to_big_endian(v);  }
		void secs(Genode::uint16_t v)  { _secs = host_to_big_endian(v); }
		void broadcast(bool v)         { _flags = v ? BROADCAST : 0; }
		void ciaddr(Ipv4_address v)    { v.copy(&_ciaddr); }
		void yiaddr(Ipv4_address v)    { v.copy(&_yiaddr); }
		void siaddr(Ipv4_address v)    { v.copy(&_siaddr); }
		void giaddr(Ipv4_address v)    { v.copy(&_giaddr); }
		void client_mac(Mac_address v) { v.copy(&_chaddr); }


		/**********************
		 ** Option utilities **
		 **********************/

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


		/*************************
		 ** Convenience methods **
		 *************************/

		static bool is_dhcp(Udp_packet const *udp)
		{
			return ((udp->src_port() == Port(Dhcp_packet::BOOTPC) ||
			         udp->src_port() == Port(Dhcp_packet::BOOTPS)) &&
			        (udp->dst_port() == Port(Dhcp_packet::BOOTPC) ||
			         udp->dst_port() == Port(Dhcp_packet::BOOTPS)));
		}


		/***************
		 ** Operators **
		 ***************/

		/**
		 * Placement new.
		 */
		void * operator new(__SIZE_TYPE__ size, void* addr) { return addr; }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _DHCP_H_ */
