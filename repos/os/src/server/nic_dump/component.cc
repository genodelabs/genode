/*
 * \brief  Downlink interface in form of a NIC session component
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
#include <os/session_policy.h>

/* local includes */
#include <component.h>
#include <uplink.h>

using namespace Net;
using namespace Genode;


/**************************
 ** Communication_buffer **
 **************************/

Communication_buffer::Communication_buffer(Ram_session          &ram,
                                           Genode::size_t const  size)
:
	Ram_dataspace_capability(ram.alloc(size)), _ram(ram)
{ }


/****************************
 ** Session_component_base **
 ****************************/

Session_component_base::
Session_component_base(Allocator    &guarded_alloc_backing,
                       size_t const  guarded_alloc_amount,
                       Ram_session  &buf_ram,
                       size_t const  tx_buf_size,
                       size_t const  rx_buf_size)
:
	_guarded_alloc(&guarded_alloc_backing, guarded_alloc_amount),
	_range_alloc(&_guarded_alloc), _tx_buf(buf_ram, tx_buf_size),
	_rx_buf(buf_ram, rx_buf_size)
{ }


/***********************
 ** Session_component **
 ***********************/

Net::Session_component::Session_component(Allocator         &alloc,
                                          size_t const       amount,
                                          Ram_session       &buf_ram,
                                          size_t const       tx_buf_size,
                                          size_t const       rx_buf_size,
                                          Region_map        &region_map,
                                          Uplink            &uplink,
                                          Xml_node           config,
                                          Timer::Connection &timer,
                                          unsigned          &curr_time,
                                          Entrypoint        &ep)
:
	Session_component_base(alloc, amount, buf_ram, tx_buf_size, rx_buf_size),
	Session_rpc_object(region_map, _tx_buf, _rx_buf, &_range_alloc, ep.rpc_ep()),
	Interface(ep, config.attribute_value("downlink", Interface_label()),
	          timer, curr_time, config.attribute_value("time", false),
	          _guarded_alloc),
	_mac(uplink.mac_address())
{
	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
	Interface::remote(uplink);
	uplink.Interface::remote(*this);
}


bool Session_component::link_state()
{
	warning("Session_component::link_state not implemented");
	return false;
}


void Session_component::link_state_sigh(Signal_context_capability sigh)
{
	warning("Session_component::link_state_sigh not implemented");
}


/**********
 ** Root **
 **********/

Net::Root::Root(Entrypoint        &ep,
                Allocator         &alloc,
                Uplink            &uplink,
                Ram_session       &buf_ram,
                Xml_node           config,
                Timer::Connection &timer,
                unsigned          &curr_time,
                Region_map        &region_map)
:
	Root_component<Session_component, Genode::Single_client>(&ep.rpc_ep(),
	                                                         &alloc),
	_ep(ep), _uplink(uplink), _buf_ram(buf_ram),
	_region_map(region_map), _config(config), _timer(timer),
	_curr_time(curr_time)
{ }


Session_component *Net::Root::_create_session(char const *args)
{
	try {
		size_t const ram_quota =
			Arg_string::find_arg(args, "ram_quota").ulong_value(0);

		size_t const tx_buf_size =
			Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

		size_t const rx_buf_size =
			Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

		size_t const session_size =
			max((size_t)4096, sizeof(Session_component));

		if (ram_quota < session_size) {
			throw Insufficient_ram_quota(); }

		if (tx_buf_size               > ram_quota - session_size ||
		    rx_buf_size               > ram_quota - session_size ||
		    tx_buf_size + rx_buf_size > ram_quota - session_size)
		{
			error("insufficient 'ram_quota' for session creation");
			throw Insufficient_ram_quota();
		}
		return new (md_alloc())
			Session_component(*md_alloc(), ram_quota - session_size,
			                  _buf_ram, tx_buf_size, rx_buf_size, _region_map,
			                  _uplink, _config, _timer, _curr_time, _ep);
	}
	catch (...) { throw Service_denied(); }
}
