/*
 * \brief  Interface back-end using NIC sessions provided by the NIC router
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
#include <nic_session_root.h>
#include <session_creation.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/********************************
 ** Nic_session_component_base **
 ********************************/

Nic_session_component_base::
Nic_session_component_base(Session_env  &session_env,
                           size_t const  tx_buf_size,
                           size_t const  rx_buf_size)
:
	_session_env  { session_env },
	_alloc        { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf       { _session_env, tx_buf_size },
	_rx_buf       { _session_env, rx_buf_size }
{ }


/*********************************************
 ** Nic_session_component::Interface_policy **
 *********************************************/

Net::Nic_session_component::
Interface_policy::Interface_policy(Genode::Session_label const &label,
                                   Session_env           const &session_env,
                                   Configuration         const &config)
:
	_label       { label },
	_config      { config },
	_session_env { session_env }
{
	_session_link_state_transition(DOWN);
}


Domain_name
Net::Nic_session_component::Interface_policy::determine_domain_name() const
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


void Net::Nic_session_component::
Interface_policy::_session_link_state_transition(Transient_link_state tls)
{
	_transient_link_state = tls;
	Signal_transmitter(_session_link_state_sigh).submit();
}


void Net::
Nic_session_component::Interface_policy::handle_domain_ready_state(bool state)
{
	if (state) {

		switch (_transient_link_state) {
		case DOWN_ACKNOWLEDGED:

			_session_link_state_transition(UP);
			break;

		case DOWN:

			_transient_link_state = DOWN_UP;
			break;

		case UP_DOWN:

			_transient_link_state = UP_DOWN_UP;
			break;

		case DOWN_UP:

			break;

		case DOWN_UP_DOWN:

			_transient_link_state = DOWN_UP;
			break;

		case UP_ACKNOWLEDGED: break;
		case UP:              break;
		case UP_DOWN_UP:      break;
		}

	} else {

		switch (_transient_link_state) {
		case UP_ACKNOWLEDGED:

			_session_link_state_transition(DOWN);
			break;

		case UP:

			_transient_link_state = UP_DOWN;
			break;

		case DOWN_UP:

			_transient_link_state = DOWN_UP_DOWN;
			break;

		case UP_DOWN:

			break;

		case UP_DOWN_UP:

			_transient_link_state = UP_DOWN;
			break;

		case DOWN_ACKNOWLEDGED: break;
		case DOWN:              break;
		case DOWN_UP_DOWN:      break;
		}
	}
}

bool
Net::Nic_session_component::Interface_policy::interface_link_state() const
{
	switch (_transient_link_state) {
	case DOWN_ACKNOWLEDGED: return false;
	case DOWN:              return false;
	case DOWN_UP:           return true;
	case DOWN_UP_DOWN:      return false;
	case UP_ACKNOWLEDGED:   return true;
	case UP:                return true;
	case UP_DOWN:           return false;
	case UP_DOWN_UP:        return true;
	}
	class Never_reached : Exception { };
	throw Never_reached { };
}


bool
Net::Nic_session_component::Interface_policy::read_and_ack_session_link_state()
{
	switch (_transient_link_state) {
	case DOWN_ACKNOWLEDGED:

		return false;

	case DOWN:

		_transient_link_state = DOWN_ACKNOWLEDGED;
		return false;

	case DOWN_UP:

		_session_link_state_transition(UP);
		return false;

	case DOWN_UP_DOWN:

		_session_link_state_transition(UP_DOWN);
		return false;

	case UP_ACKNOWLEDGED:

		return true;

	case UP:

		_transient_link_state = UP_ACKNOWLEDGED;
		return true;

	case UP_DOWN:

		_session_link_state_transition(DOWN);
		return true;

	case UP_DOWN_UP:

		_session_link_state_transition(DOWN_UP);
		return true;
	}
	class Never_reached { };
	throw Never_reached { };
}


void Net::Nic_session_component::Interface_policy::
session_link_state_sigh(Signal_context_capability sigh)
{
	_session_link_state_sigh = sigh;
}


/***************************
 ** Nic_session_component **
 ***************************/

Net::
Nic_session_component::
Nic_session_component(Session_env                    &session_env,
                      size_t                   const  tx_buf_size,
                      size_t                   const  rx_buf_size,
                      Cached_timer                   &timer,
                      Mac_address              const  mac,
                      Mac_address              const &router_mac,
                      Session_label            const &label,
                      Interface_list                 &interfaces,
                      Configuration                  &config,
                      Ram_dataspace_capability const  ram_ds)
:
	Nic_session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object         { _session_env, _tx_buf.ds(), _rx_buf.ds(),
	                             &_packet_alloc, _session_env.ep().rpc_ep() },
	_interface_policy          { label, _session_env, config },
	_interface                 { _session_env.ep(), timer, router_mac, _alloc,
	                             mac, config, interfaces, *_tx.sink(),
	                             *_rx.source(), _interface_policy },
	_ram_ds                    { ram_ds }
{
	_interface.attach_to_domain();

	/* install packet stream signal handlers */
	_tx.sigh_packet_avail(_interface.pkt_stream_signal_handler());
	_rx.sigh_ack_avail   (_interface.pkt_stream_signal_handler());

	/*
	 * We do not install ready_to_submit because submission is only triggered by
	 * incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */
}


bool Net::Nic_session_component::link_state()
{
	return _interface_policy.read_and_ack_session_link_state();
}


void Net::
Nic_session_component::link_state_sigh(Signal_context_capability sigh)
{
	_interface_policy.session_link_state_sigh(sigh);
}


/**********************
 ** Nic_session_root **
 **********************/

Net::Nic_session_root::Nic_session_root(Env               &env,
                                        Cached_timer      &timer,
                                        Allocator         &alloc,
                                        Configuration     &config,
                                        Quota             &shared_quota,
                                        Interface_list    &interfaces)
:
	Root_component<Nic_session_component> { &env.ep().rpc_ep(), &alloc },
	_env                                  { env },
	_timer                                { timer },
	_mac_alloc                            { MAC_ALLOC_BASE },
	_router_mac                           { _mac_alloc.alloc() },
	_config                               { config },
	_shared_quota                         { shared_quota },
	_interfaces                           { interfaces }
{ }


Nic_session_component *Net::Nic_session_root::_create_session(char const *args)
{
	Session_creation<Nic_session_component> session_creation { };
	try {
		return session_creation.execute(
			_env, _shared_quota, args,
			[&] (Session_env &session_env, void *session_at, Ram_dataspace_capability ram_ds)
			{
				Session_label const label { label_from_args(args) };
				Mac_address const mac { _mac_alloc.alloc() };
				try {
					return construct_at<Nic_session_component>(
						session_at, session_env,
						Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
						Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
						_timer, mac, _router_mac, label, _interfaces,
						_config(), ram_ds);
				}
				catch (...) {
					_mac_alloc.free(mac);
					throw;
				}
			});
	}
	catch (Mac_allocator::Alloc_failed) {
		_invalid_downlink("failed to allocate MAC address");
		throw Service_denied();
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
		_invalid_downlink("NIC session RAM quota");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		_invalid_downlink("NIC session CAP quota");
		throw Insufficient_cap_quota();
	}
}

void Net::Nic_session_root::_destroy_session(Nic_session_component *session)
{
	Mac_address const mac = session->mac_address();

	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability  ram_ds        { session->ram_ds() };
	Session_env        const &session_env   { session->session_env() };
	Session_label      const  session_label { session->interface_policy().label() };
	session->~Nic_session_component();

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


void Net::Nic_session_root::_invalid_downlink(char const *reason)
{
	if (_config().verbose()) {
		log("[?] invalid downlink (", reason, ")"); }
}
