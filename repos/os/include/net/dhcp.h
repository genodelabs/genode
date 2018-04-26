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
#include <util/construct_at.h>

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

		enum Opcode {
			REQUEST = 1,
			REPLY   = 2,
			INVALID
		};

		enum Udp_port {
			BOOTPS = 67,
			BOOTPC = 68
		};

		void default_magic_cookie() {
			_magic_cookie = host_to_big_endian(0x63825363);
		}


		/*******************************
		 ** Utilities for the options **
		 *******************************/

		/**
		 * Header of a DHCP option or DHCP option without a payload
		 */
		class Option
		{
			private:

				Genode::uint8_t _code;
				Genode::uint8_t _len;
				Genode::uint8_t _value[0];

			public:

				enum class Code : Genode::uint8_t {
					INVALID        = 0,
					SUBNET_MASK    = 1,
					ROUTER         = 3,
					DNS_SERVER     = 6,
					BROADCAST_ADDR = 28,
					REQ_IP_ADDR    = 50,
					IP_LEASE_TIME  = 51,
					MSG_TYPE       = 53,
					SERVER         = 54,
					PARAM_REQ_LIST = 55,
					MAX_MSG_SZ     = 57,
					CLI_ID         = 61,
					END            = 255,
				};

				Option(Code code, Genode::uint8_t len)
				: _code((Genode::uint8_t)code), _len(len) { }

				Code             code() const { return (Code)_code; }
				Genode::uint8_t len()   const { return _len; }


				/*********
				 ** log **
				 *********/

				void print(Genode::Output &output) const;

		} __attribute__((packed));

		struct Option_not_found : Genode::Exception
		{
			Option::Code const code;

			Option_not_found(Option::Code code) : code(code) { }
		};

		/**
		 * DHCP option that contains a payload of type T
		 */
		template <typename T>
		class Option_tpl : public Option
		{
			protected:

				T _value;

			public:

				Option_tpl(Code code, T value)
				: Option(code, sizeof(T)), _value(value) { }

		} __attribute__((packed));


		/**
		 * DHCP option that specifies the IP packet lease time in seconds
		 */
		struct Ip_lease_time : Option_tpl<Genode::uint32_t>
		{
			static constexpr Code CODE = Code::IP_LEASE_TIME;

			Ip_lease_time(Genode::uint32_t time)
			: Option_tpl(CODE, host_to_big_endian(time)) { }

			unsigned long value() const { return host_to_big_endian(_value); }
		};

		/**
		 * DHCP option to request specific option type values from the server
		 */
		struct Parameter_request_list : Option
		{
			static constexpr Code CODE = Code::PARAM_REQ_LIST;

			Parameter_request_list(Genode::size_t len) : Option(CODE, len) { }
		};

		enum class Message_type : Genode::uint8_t {
			DISCOVER  = 1,
			OFFER     = 2,
			REQUEST   = 3,
			DECLINE   = 4,
			ACK       = 5,
			NAK       = 6,
			RELEASE   = 7,
			INFORM    = 8
		};

		/**
		 * DHCP option that specifies the DHCP message type
		 */
		struct Message_type_option : Option_tpl<Genode::uint8_t>
		{
			static constexpr Code CODE = Code::MSG_TYPE;

			Message_type_option(Message_type value)
			: Option_tpl(CODE, (Genode::uint8_t)value) { }

			Message_type value() const { return (Message_type)_value; }
		};


		/**
		 * DHCP options that have only one IPv4 address as payload
		 */
		template <Option::Code _CODE>
		struct Ipv4_option : Option_tpl<Genode::uint32_t>
		{
			static constexpr Code CODE = _CODE;

			Ipv4_option(Ipv4_address value)
			: Option_tpl(CODE, value.to_uint32_big_endian()) { }

			Ipv4_address value() const {
				return Ipv4_address::from_uint32_big_endian(_value); }
		};

		using Dns_server_ipv4 = Ipv4_option<Option::Code::DNS_SERVER>;
		using Subnet_mask     = Ipv4_option<Option::Code::SUBNET_MASK>;
		using Broadcast_addr  = Ipv4_option<Option::Code::BROADCAST_ADDR>;
		using Router_ipv4     = Ipv4_option<Option::Code::ROUTER>;
		using Server_ipv4     = Ipv4_option<Option::Code::SERVER>;
		using Requested_addr  = Ipv4_option<Option::Code::REQ_IP_ADDR>;


		class Client_id
		{
			private:

				Genode::uint8_t _code;
				Genode::uint8_t _len;
				Genode::uint8_t _value[7];

			public:

				Client_id(Mac_address value)
				: _code((Genode::uint8_t)Option::Code::CLI_ID), _len(7)
				{
					_value[0] = 1;
					_value[1] = value.addr[0];
					_value[2] = value.addr[1];
					_value[3] = value.addr[2];
					_value[4] = value.addr[3];
					_value[5] = value.addr[4];
					_value[6] = value.addr[5];
				}

				Genode::uint8_t code() const { return _code; }
				Genode::uint8_t len()  const { return _len; }
		} __attribute__((packed));


		struct Max_msg_size : Option_tpl<Genode::uint16_t>
		{
			static constexpr Code CODE = Code::MAX_MSG_SZ;

			Max_msg_size(Genode::uint16_t size)
			: Option_tpl(CODE, host_to_big_endian(size)) { }
		};


		/**
		 * DHCP option that marks the end of an options field
		 */
		struct Options_end : Option
		{
			static constexpr Code CODE = Code::END;

			Options_end() : Option(CODE, 0) { }
		};


		/**
		 * Utility to append individual options to an existing DHCP packet
		 *
		 * \param SIZE_GUARD  guard that may limit the options list size
		 *
		 * Overwrites existing options if any!
		 */
		template <typename SIZE_GUARD>
		class Options_aggregator
		{
			private:

				Genode::addr_t  _base;
				SIZE_GUARD     &_size_guard;

			public:

				class Parameter_request_list_data
				{
					private:

						Genode::uint8_t *const  _base;
						Genode::size_t          _size { 0 };
						SIZE_GUARD             &_size_guard;

					public:

						Parameter_request_list_data(Genode::uint8_t *base,
						                            SIZE_GUARD      &size_guard)
						:
							_base       { base },
							_size_guard { size_guard }
						{ }

						template <typename OPTION>
						void append_param_req()
						{
							_size_guard.consume_head(sizeof(_base[0]));
							_base[_size] = (Genode::uint8_t)OPTION::CODE;
							_size++;
						}

						Genode::size_t size() const { return _size; }
				};

				Options_aggregator(Dhcp_packet &packet,
				                   SIZE_GUARD  &size_guard)
				:
					_base((Genode::addr_t)packet.opts()),
					_size_guard(size_guard)
				{ }

				template <typename OPTION, typename... ARGS>
				void append_option(ARGS &&... args)
				{
					_size_guard.consume_head(sizeof(OPTION));
					Genode::construct_at<OPTION>((void *)_base,
					                             static_cast<ARGS &&>(args)...);
					_base += sizeof(OPTION);
				}

				template <typename INIT_DATA>
				void append_param_req_list(INIT_DATA && init_data)
				{
					_size_guard.consume_head(sizeof(Parameter_request_list));
					Parameter_request_list_data
						data((Genode::uint8_t *)(_base + sizeof(Parameter_request_list)),
						     _size_guard);

					init_data(data);
					Genode::construct_at<Parameter_request_list>((void *)_base,
					                                             data.size());

					_base += sizeof(Parameter_request_list) + data.size();
				}
		};


		/*
		 * Call 'functor' of type 'FUNC' for each option (except END options)
		 */
		template <typename FUNC>
		void for_each_option(FUNC && functor) const
		{
			for (unsigned i = 0; ; ) {
				Option &opt = *(Option*)&_opts[i];
				if (opt.code() == Option::Code::INVALID ||
				    opt.code() == Option::Code::END)
				{
					return;
				}
				functor(opt);
				i += 2 + opt.len();
			}
		}


		/*
		 * Find and return option of given type 'T'
		 *
		 * \throw Option_not_found
		 */
		template <typename T>
		T &option()
		{
			void *ptr = &_opts;
			while (true) {
				Option &opt = *reinterpret_cast<Option *>(ptr);
				if (opt.code() == Option::Code::INVALID ||
				    opt.code() == Option::Code::END)
				{
					throw Option_not_found(T::CODE);
				}
				if (opt.code() == T::CODE) {
					return *reinterpret_cast<T *>(ptr);
				}
				ptr = (void *)((Genode::addr_t)ptr + sizeof(opt) + opt.len());
			}
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


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;

} __attribute__((packed));

#endif /* _DHCP_H_ */
