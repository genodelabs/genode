/*
 * \brief  Uplink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UPLINK_H_
#define _UPLINK_H_

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include <interface.h>

namespace Net { class Uplink; }

class Net::Uplink : public Nic::Packet_allocator,
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

		Uplink(Genode::Entrypoint &ep,
		       Genode::Timer      &timer,
		       Genode::Allocator  &alloc,
		       Configuration      &config);


		/***************
		 ** Accessors **
		 ***************/

		Mac_address const &router_mac() const { return _router_mac; }
};

#endif /* _UPLINK_H_ */
