/*
 * \brief  Uplink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2017-03-08
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
#include <util/xml_node.h>

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

		/********************
		 ** Net::Interface **
		 ********************/

		Packet_stream_sink   &_sink()   { return *rx(); }
		Packet_stream_source &_source() { return *tx(); }

	public:

		Uplink(Genode::Env       &env,
		       Genode::Xml_node   config,
		       Timer::Connection &timer,
		       Genode::Duration  &curr_time,
		       Genode::Allocator &alloc);
};

#endif /* _UPLINK_H_ */
