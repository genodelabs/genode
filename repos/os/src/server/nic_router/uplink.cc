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

/* Genode includes */
#include <base/env.h>
#include <net/ethernet.h>

/* local includes */
#include <uplink.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


Net::Uplink::Uplink(Env                 &env,
                    Timer::Connection   &timer,
                    Genode::Allocator   &alloc,
                    Interface_list      &interfaces,
                    Configuration       &config,
                    Session_label const &label)
:
	Nic::Packet_allocator(&alloc),
	Nic::Connection(env, this, BUF_SIZE, BUF_SIZE, label.string()),
	Net::Interface(env.ep(), timer, mac_address(), alloc, Mac_address(),
	               config, interfaces, _intf_policy),
	_label(label),
	_link_state_handler(env.ep(), *this, &Uplink::_handle_link_state)
{
	rx_channel()->sigh_ready_to_ack(_sink_ack);
	rx_channel()->sigh_packet_avail(_sink_submit);
	tx_channel()->sigh_ack_avail(_source_ack);
	tx_channel()->sigh_ready_to_submit(_source_submit);
	Nic::Connection::link_state_sigh(_link_state_handler);
	_link_state_ = link_state();
}


void Net::Uplink::_handle_link_state()
{
	_link_state_ = link_state();
	try { domain().discard_ip_config(); }
	catch (Domain::Ip_config_static) { }
}
