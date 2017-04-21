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

/* Genode includes */
#include <base/env.h>
#include <net/ethernet.h>

/* local includes */
#include <uplink.h>

using namespace Net;
using namespace Genode;


Net::Uplink::Uplink(Env               &env,
                    Xml_node           config,
                    Timer::Connection &timer,
                    unsigned          &curr_time,
                    Allocator         &alloc)
:
	Nic::Packet_allocator(&alloc),
	Nic::Connection(env, this, BUF_SIZE, BUF_SIZE),
	Interface(env.ep(), config.attribute_value("uplink", Interface_label()),
	          timer, curr_time, config.attribute_value("time", false), alloc)
{
	rx_channel()->sigh_ready_to_ack(_sink_ack);
	rx_channel()->sigh_packet_avail(_sink_submit);
	tx_channel()->sigh_ack_avail(_source_ack);
	tx_channel()->sigh_ready_to_submit(_source_submit);
}
