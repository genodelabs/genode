/*
 * \brief  Interface back-end using Uplink sessions provided by the NIC router
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
#include <os/session_policy.h>

/* local includes */
#include <uplink_session_root.h>
#include <configuration.h>
#include <session_creation.h>

using namespace Net;
using namespace Genode;


/***********************************
 ** Uplink_session_component_base **
 ***********************************/

Uplink_session_component_base::
Uplink_session_component_base(Session_env  &session_env,
                              size_t const  tx_buf_size,
                              size_t const  rx_buf_size)
:
	_session_env  { session_env },
	_alloc        { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf       { _session_env, tx_buf_size },
	_rx_buf       { _session_env, rx_buf_size }
{ }


/************************************************
 ** Uplink_session_component::Interface_policy **
 ************************************************/

Net::Uplink_session_component::
Interface_policy::Interface_policy(Genode::Session_label const &label,
                                   Session_env           const &session_env,
                                   Configuration         const &config)
:
	_label       { label },
	_config      { config },
	_session_env { session_env }
{ }


Domain_name
Net::Uplink_session_component::Interface_policy::determine_domain_name() const
{
	Domain_name domain_name;
	try {
		Session_policy policy(_label, _config().node());
		domain_name = policy.attribute_value("domain", Domain_name());
	}
	catch (Session_policy::No_policy_defined) {
		if (_config().verbose()) {
			log("[?] no policy for downlink label \"", _label, "\""); }
	}
	catch (Xml_node::Nonexistent_attribute) {
		if (_config().verbose()) {
			log("[?] no domain attribute in policy for downlink label \"",
			    _label, "\"");
		}
	}
	return domain_name;
}


/******************************
 ** Uplink_session_component **
 ******************************/

Net::Uplink_session_component::Uplink_session_component(Session_env                    &session_env,
                                                        size_t                   const  tx_buf_size,
                                                        size_t                   const  rx_buf_size,
                                                        Cached_timer                   &timer,
                                                        Mac_address              const  mac,
                                                        Session_label            const &label,
                                                        Interface_list                 &interfaces,
                                                        Configuration                  &config,
                                                        Ram_dataspace_capability const  ram_ds)
:
	Uplink_session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object            { _session_env, _tx_buf.ds(), _rx_buf.ds(),
	                                &_packet_alloc, _session_env.ep().rpc_ep() },
	_interface_policy             { label, _session_env, config },
	_interface                    { _session_env.ep(), timer, mac, _alloc,
	                                Mac_address(), config, interfaces, *_tx.sink(),
	                                *_rx.source(), _interface_policy },
	_ram_ds                       { ram_ds }
{
	_interface.attach_to_domain();

	/* install packet stream signal handlers */
	_tx.sigh_packet_avail(_interface.pkt_stream_signal_handler());
	_rx.sigh_ack_avail   (_interface.pkt_stream_signal_handler());

	/*
	 * We do not install ready_to_submit because submission is only triggered
	 * by incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */
}


/*************************
 ** Uplink_session_root **
 *************************/

Net::Uplink_session_root::Uplink_session_root(Env               &env,
                                              Cached_timer      &timer,
                                              Allocator         &alloc,
                                              Configuration     &config,
                                              Quota             &shared_quota,
                                              Interface_list    &interfaces)
:
	Root_component<Uplink_session_component> { &env.ep().rpc_ep(), &alloc },
	_env                                     { env },
	_timer                                   { timer },
	_config                                  { config },
	_shared_quota                            { shared_quota },
	_interfaces                              { interfaces }
{ }


Uplink_session_component *
Net::Uplink_session_root::_create_session(char const *args)
{
	Session_creation<Uplink_session_component> session_creation { };
	try {
		return session_creation.execute(
			_env, _shared_quota, args,
			[&] (Session_env &session_env, void *session_at, Ram_dataspace_capability ram_ds)
			{
				Session_label const label { label_from_args(args) };

				enum { MAC_STR_LENGTH = 19 };
				char mac_str [MAC_STR_LENGTH];
				Arg mac_arg { Arg_string::find_arg(args, "mac_address") };

				if (!mac_arg.valid()) {
					_invalid_downlink("failed to find 'mac_address' arg");
					throw Service_denied();
				}
				mac_arg.string(mac_str, MAC_STR_LENGTH, "");
				Mac_address mac { };
				ascii_to(mac_str, mac);
				if (mac == Mac_address { }) {
					_invalid_downlink("malformed 'mac_address' arg");
					throw Service_denied();
				}
				return construct_at<Uplink_session_component>(
					session_at, session_env,
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
					Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
					_timer, mac, label, _interfaces, _config(), ram_ds);
			});
	}
	catch (Region_map::Invalid_dataspace) {
		_invalid_downlink("Failed to attach RAM");
		throw Service_denied();
	}
	catch (Region_map::Region_conflict) {
		_invalid_downlink("Failed to attach RAM");
		throw Service_denied();
	}
	catch (Out_of_ram) {
		_invalid_downlink("Uplink session RAM quota");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		_invalid_downlink("Uplink session CAP quota");
		throw Insufficient_cap_quota();
	}
}

void
Net::Uplink_session_root::_destroy_session(Uplink_session_component *session)
{
	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability  ram_ds        { session->ram_ds() };
	Session_env        const &session_env   { session->session_env() };
	Session_label      const  session_label { session->interface_policy().label() };
	session->~Uplink_session_component();

	/* copy session env to stack and detach/free all session data */
	Session_env session_env_stack { session_env };
	session_env_stack.detach(session);
	session_env_stack.detach(&session_env);
	session_env_stack.free(ram_ds);

	/* check for leaked quota */
	if (session_env_stack.ram_guard().used().value) {
		error("Uplink session component \"", session_label, "\" leaks RAM quota of ",
		      session_env_stack.ram_guard().used().value, " byte(s)"); };
	if (session_env_stack.cap_guard().used().value) {
		error("Uplink session component \"", session_label, "\" leaks CAP quota of ",
		      session_env_stack.cap_guard().used().value, " cap(s)"); };
}


void Net::Uplink_session_root::_invalid_downlink(char const *reason)
{
	if (_config().verbose()) {
		log("[?] invalid downlink (", reason, ")"); }
}
