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

Communication_buffer::Communication_buffer(Ram_allocator &ram,
                                           Genode::size_t const size)
:
	Ram_dataspace_capability(ram.alloc(size)), _ram(ram)
{ }


/****************************
 ** Session_component_base **
 ****************************/

Session_component_base::
Session_component_base(Ram_allocator  &ram,
                       Region_map     &local_rm,
                       Ram_quota const ram_quota,
                       Cap_quota const cap_quota,
                       size_t    const tx_buf_size,
                       size_t    const rx_buf_size)
:
	_ram_quota_guard(ram_quota),
	_cap_quota_guard(cap_quota),
	_ram(ram, _ram_quota_guard, _cap_quota_guard),
	_alloc(_ram, local_rm),
	_range_alloc(&_alloc),
	_tx_buf(ram, tx_buf_size),
	_rx_buf(ram, rx_buf_size)
{ }


/***********************
 ** Session_component **
 ***********************/

Net::Session_component::Session_component(Ram_quota    const ram_quota,
                                          Cap_quota    const cap_quota,
                                          size_t       const tx_buf_size,
                                          size_t       const rx_buf_size,
                                          Xml_node     const config,
                                          Timer::Connection &timer,
                                          Duration          &curr_time,
                                          Env               &env)
:
	Session_component_base(env.ram(), env.rm(), ram_quota, cap_quota,
	                       tx_buf_size, rx_buf_size),
	Session_rpc_object(env.rm(), _tx_buf, _rx_buf, &_range_alloc,
	                   env.ep().rpc_ep()),
	Interface(env.ep(), config.attribute_value("downlink", Interface_label()),
	          timer, curr_time, config.attribute_value("time", false),
	          Session_component_base::_alloc, config),
	_uplink(env, config, timer, curr_time, Session_component_base::_alloc),
	_link_state_handler(env.ep(), *this, &Session_component::_handle_link_state)
{
	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
	Interface::remote(_uplink);
	_uplink.Interface::remote(*this);
	_uplink.link_state_sigh(_link_state_handler);
	_print_state();
}


void Session_component::_print_state()
{
	log("\033[33m(new state)\033[0m \033[32mMAC address\033[0m ", mac_address(),
	                              " \033[32mlink state\033[0m ",  link_state());
}


void Session_component::_handle_link_state()
{
	_print_state();
	if (!_link_state_sigh.valid()) {
		warning("failed to forward signal");
		return;
	}
	Signal_transmitter(_link_state_sigh).submit();
}


/**********
 ** Root **
 **********/

Net::Root::Root(Env               &env,
                Allocator         &alloc,
                Xml_node           config,
                Timer::Connection &timer,
                Duration          &curr_time)
:
	Root_component<Session_component, Genode::Single_client>(&env.ep().rpc_ep(),
	                                                         &alloc),
	_env(env), _config(config), _timer(timer), _curr_time(curr_time)
{ }


Session_component *Net::Root::_create_session(char const *args)
{
	try {
		Ram_quota const ram_quota = ram_quota_from_args(args);
		Cap_quota const cap_quota = cap_quota_from_args(args);

		size_t const tx_buf_size =
			Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

		size_t const rx_buf_size =
			Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

		size_t const session_size =
			max((size_t)4096, sizeof(Session_component));

		if (ram_quota.value < session_size) {
			throw Insufficient_ram_quota(); }

		if (tx_buf_size               > ram_quota.value - session_size ||
		    rx_buf_size               > ram_quota.value - session_size ||
		    tx_buf_size + rx_buf_size > ram_quota.value - session_size)
		{
			error("insufficient 'ram_quota' for session creation");
			throw Insufficient_ram_quota();
		}
		return new (md_alloc())
			Session_component(Ram_quota{ram_quota.value - session_size},
			                  cap_quota, tx_buf_size, rx_buf_size, _config, _timer,
			                  _curr_time, _env);
	}
	catch (...) { throw Service_denied(); }
}
