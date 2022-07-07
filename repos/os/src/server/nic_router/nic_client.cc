/*
 * \brief  Interface back-end using a NIC session requested by the NIC router
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

/* local includes */
#include <nic_client.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/*********************
 ** Nic_client_base **
 *********************/

Net::Nic_client_base::Nic_client_base(Xml_node const &node)
:
	_label  { node.attribute_value("label",  Session_label::String()) },
	_domain { node.attribute_value("domain", Domain_name()) }
{ }


/****************
 ** Nic_client **
 ****************/

void Nic_client::_invalid(char const *reason) const
{
	if (_config.verbose()) {
		log("[", domain(), "] invalid NIC client: ", *this, " (", reason, ")"); }

	throw Invalid();
}


Net::Nic_client::Nic_client(Xml_node    const &node,
                            Allocator         &alloc,
                            Nic_client_tree   &old_nic_clients,
                            Env               &env,
                            Cached_timer      &timer,
                            Interface_list    &interfaces,
                            Configuration     &config)
:
	Nic_client_base { node },
	Avl_string_base { label().string() },
	_alloc          { alloc },
	_config         { config }
{
	old_nic_clients.find_by_name(
		label(),
		[&] /* handle_match */ (Nic_client &old_nic_client)
		{
			/* reuse existing interface */
			Nic_client_interface &interface = old_nic_client._interface();
			old_nic_client._interface = Pointer<Nic_client_interface>();
			interface.domain_name(domain());
			_interface = interface;
		},
		[&] /* handle_no_match */ ()
		{
			/* create a new interface */
			if (config.verbose()) {
				log("[", domain(), "] create NIC client: ", *this); }

			try {
				_interface = *new (_alloc)
					Nic_client_interface {
						env, timer, alloc, interfaces, config, domain(),
						label() };
			}
			catch (Insufficient_ram_quota) { _invalid("NIC session RAM quota"); }
			catch (Insufficient_cap_quota) { _invalid("NIC session CAP quota"); }
			catch (Service_denied)         { _invalid("NIC session denied"); }
		}
	);
}


Net::Nic_client::~Nic_client()
{
	/* if the interface was yet not reused by another NIC client, destroy it */
	try {
		Nic_client_interface &interface = _interface();
		if (_config.verbose()) {
			log("[", domain(), "] destroy NIC client: ", *this); }

		destroy(_alloc, &interface);
	}
	catch (Pointer<Nic_client_interface>::Invalid) { }
}


void Net::Nic_client::print(Output &output) const
{
	if (label() == Session_label()) {
		Genode::print(output, "?"); }
	else {
		Genode::print(output, label()); }
}


/*******************************
 ** Nic_client_interface_base **
 *******************************/

Net::Nic_client_interface_base::
     Nic_client_interface_base(Domain_name   const &domain_name,
                               Session_label const &label,
                               bool          const &session_link_state)
:
	_domain_name        { domain_name },
	_label              { label },
	_session_link_state { session_link_state }
{ }


void Net::Nic_client_interface_base::interface_unready()
{
	_interface_ready = false;
};


void Net::Nic_client_interface_base::interface_ready()
{
	_interface_ready = true;
};


bool Net::Nic_client_interface_base::interface_link_state() const
{
	return _interface_ready && _session_link_state;
}


/**************************
 ** Nic_client_interface **
 **************************/

Net::Nic_client_interface::Nic_client_interface(Env                 &env,
                                                Cached_timer        &timer,
                                                Genode::Allocator   &alloc,
                                                Interface_list      &interfaces,
                                                Configuration       &config,
                                                Domain_name   const &domain_name,
                                                Session_label const &label)
:
	Nic_client_interface_base   { domain_name, label, _session_link_state },
	Nic::Packet_allocator       { &alloc },
	Nic::Connection             { env, this, BUF_SIZE, BUF_SIZE, label.string() },
	_session_link_state_handler { env.ep(), *this,
	                              &Nic_client_interface::_handle_session_link_state },
	_interface                  { env.ep(), timer, mac_address(), alloc,
	                              Mac_address(), config, interfaces, *rx(), *tx(),
	                              *this }
{
	/* install packet stream signal handlers */
	rx_channel()->sigh_packet_avail(_interface.pkt_stream_signal_handler());
	tx_channel()->sigh_ack_avail   (_interface.pkt_stream_signal_handler());

	/*
	 * We do not install ready_to_submit because submission is only triggered
	 * by incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */

	/* initialize link state handling */
	Nic::Connection::link_state_sigh(_session_link_state_handler);
	_session_link_state = Nic::Connection::link_state();
}


void Net::Nic_client_interface::_handle_session_link_state()
{
	_session_link_state = Nic::Connection::link_state();
	_interface.handle_interface_link_state();
}
