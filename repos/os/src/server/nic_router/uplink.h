/*
 * \brief  Uplink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_H_
#define _UPLINK_H_

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include <interface.h>
#include <ipv4_address_prefix.h>

namespace Net {

	using Domain_name = Genode::String<160>;
	class Uplink_base;
	class Uplink;
}


class Net::Uplink_base
{
	protected:

		struct Interface_policy : Net::Interface_policy
		{
			/***************************
			 ** Net::Interface_policy **
			 ***************************/

			Domain_name determine_domain_name() const override { return Genode::Cstring("uplink"); };
		};

		Interface_policy _intf_policy { };

		virtual ~Uplink_base() { }
};


class Net::Uplink : public Uplink_base,
                    public Nic::Packet_allocator,
                    public Nic::Connection,
                    public Interface
{
	private:

		enum {
			PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE = Nic::Session::QUEUE_SIZE * PKT_SIZE,
		};

		Ipv4_address_prefix _read_interface();


		/********************
		 ** Net::Interface **
		 ********************/

		Packet_stream_sink   &_sink()   { return *rx(); }
		Packet_stream_source &_source() { return *tx(); }

	public:

		Uplink(Genode::Env        &env,
		       Timer::Connection  &timer,
		       Genode::Allocator  &alloc,
		       Configuration      &config);


		/***************
		 ** Accessors **
		 ***************/

		Mac_address const &router_mac() const { return _router_mac; }
};

#endif /* _UPLINK_H_ */
