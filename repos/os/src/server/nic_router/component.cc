/*
 * \brief  Downlink interface in form of a NIC session component
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
#include <component.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/**************************
 ** Communication_buffer **
 **************************/

Communication_buffer::Communication_buffer(Ram_allocator &ram_alloc,
                                           size_t const   size)
:
	_ram_alloc { ram_alloc },
	_ram_ds    { ram_alloc.alloc(size) }
{ }


/****************************
 ** Session_component_base **
 ****************************/

Session_component_base::
Session_component_base(Session_env  &session_env,
                       size_t const  tx_buf_size,
                       size_t const  rx_buf_size)
:
	_session_env  { session_env },
	_alloc        { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf       { _session_env, tx_buf_size },
	_rx_buf       { _session_env, rx_buf_size }
{ }


/*****************************************
 ** Session_component::Interface_policy **
 *****************************************/

Net::Session_component::
Interface_policy::Interface_policy(Genode::Session_label const &label,
                                   Session_env           const &session_env,
                                   Configuration         const &config)
:
	_label       { label },
	_config      { config },
	_session_env { session_env }
{ }


Domain_name
Net::Session_component::Interface_policy::determine_domain_name() const
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


/***********************
 ** Session_component **
 ***********************/

Net::Session_component::Session_component(Session_env                    &session_env,
                                          size_t                   const  tx_buf_size,
                                          size_t                   const  rx_buf_size,
                                          Timer::Connection              &timer,
                                          Mac_address              const  mac,
                                          Mac_address              const &router_mac,
                                          Session_label            const &label,
                                          Interface_list                 &interfaces,
                                          Configuration                  &config,
                                          Ram_dataspace_capability const  ram_ds)
:
	Session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object     { _session_env, _tx_buf.ds(), _rx_buf.ds(),
	                         &_packet_alloc, _session_env.ep().rpc_ep() },
	_interface_policy      { label, _session_env, config },
	_interface             { _session_env.ep(), timer, router_mac, _alloc,
	                         mac, config, interfaces, *_tx.sink(),
	                         *_rx.source(), _link_state, _interface_policy },
	_ram_ds                { ram_ds }
{
	_interface.attach_to_domain();

	_tx.sigh_ready_to_ack   (_interface.sink_ack());
	_tx.sigh_packet_avail   (_interface.sink_submit());
	_rx.sigh_ack_avail      (_interface.source_ack());
	_rx.sigh_ready_to_submit(_interface.source_submit());
}


/**********
 ** Root **
 **********/

Net::Root::Root(Env               &env,
                Timer::Connection &timer,
                Allocator         &alloc,
                Configuration     &config,
                Quota             &shared_quota,
                Interface_list    &interfaces)
:
	Root_component<Session_component> { &env.ep().rpc_ep(), &alloc },
	_env                              { env },
	_timer                            { timer },
	_mac_alloc                        { MAC_ALLOC_BASE },
	_router_mac                       { _mac_alloc.alloc() },
	_config                           { config },
	_shared_quota                     { shared_quota },
	_interfaces                       { interfaces }
{ }


Session_component *Net::Root::_create_session(char const *args)
{
	try {
		/* create session environment temporarily on the stack */
		Session_env session_env_stack { _env, _shared_quota,
			Ram_quota { Arg_string::find_arg(args, "ram_quota").ulong_value(0) },
			Cap_quota { Arg_string::find_arg(args, "cap_quota").ulong_value(0) } };

		/* alloc/attach RAM block and move session env to base of the block */
		Ram_dataspace_capability ram_ds {
			session_env_stack.alloc(sizeof(Session_env) +
			                        sizeof(Session_component), CACHED) };
		try {
			void * const ram_ptr { session_env_stack.attach(ram_ds) };
			Session_env &session_env {
				*construct_at<Session_env>(ram_ptr, session_env_stack) };

			/* create new session object behind session env in the RAM block */
			try {
				Session_label const label { label_from_args(args) };
				Mac_address   const mac   { _mac_alloc.alloc() };
				try {
					Session_component *x = construct_at<Session_component>(
						(void*)((addr_t)ram_ptr + sizeof(Session_env)),
						session_env,
						Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
						Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
						_timer, mac, _router_mac, label, _interfaces,
						_config(), ram_ds);
					return x;
				}
				catch (Out_of_ram) {
					_mac_alloc.free(mac);
					Session_env session_env_stack { session_env };
					session_env_stack.detach(ram_ptr);
					session_env_stack.free(ram_ds);
					_invalid_downlink("NIC session RAM quota");
					throw Insufficient_ram_quota();
				}
				catch (Out_of_caps) {
					_mac_alloc.free(mac);
					Session_env session_env_stack { session_env };
					session_env_stack.detach(ram_ptr);
					session_env_stack.free(ram_ds);
					_invalid_downlink("NIC session CAP quota");
					throw Insufficient_cap_quota();
				}
			}
			catch (Mac_allocator::Alloc_failed) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				_invalid_downlink("failed to allocate MAC address");
				throw Service_denied();
			}
		}
		catch (Region_map::Invalid_dataspace) {
			session_env_stack.free(ram_ds);
			_invalid_downlink("Failed to attach RAM");
			throw Service_denied();
		}
		catch (Region_map::Region_conflict) {
			session_env_stack.free(ram_ds);
			_invalid_downlink("Failed to attach RAM");
			throw Service_denied();
		}
		catch (Out_of_ram) {
			session_env_stack.free(ram_ds);
			_invalid_downlink("NIC session RAM quota");
			throw Insufficient_ram_quota();
		}
		catch (Out_of_caps) {
			session_env_stack.free(ram_ds);
			_invalid_downlink("NIC session CAP quota");
			throw Insufficient_cap_quota();
		}
	}
	catch (Out_of_ram) {
		_invalid_downlink("NIC session RAM quota");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		_invalid_downlink("NIC session CAP quota");
		throw Insufficient_cap_quota();
	}
}

void Net::Root::_destroy_session(Session_component *session)
{
	Mac_address const mac = session->mac_address();

	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability  ram_ds        { session->ram_ds() };
	Session_env        const &session_env   { session->session_env() };
	Session_label      const  session_label { session->interface_policy().label() };
	session->~Session_component();

	/* copy session env to stack and detach/free all session data */
	Session_env session_env_stack { session_env };
	session_env_stack.detach(session);
	session_env_stack.detach(&session_env);
	session_env_stack.free(ram_ds);

	_mac_alloc.free(mac);

	/* check for leaked quota */
	if (session_env_stack.ram_guard().used().value) {
		error("NIC session component \"", session_label, "\" leaks RAM quota of ",
		      session_env_stack.ram_guard().used().value, " byte(s)"); };
	if (session_env_stack.cap_guard().used().value) {
		error("NIC session component \"", session_label, "\" leaks CAP quota of ",
		      session_env_stack.cap_guard().used().value, " cap(s)"); };
}


void Net::Root::_invalid_downlink(char const *reason)
{
	if (_config().verbose()) {
		log("[?] invalid downlink (", reason, ")"); }
}
