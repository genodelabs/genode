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


/****************
 ** Nic_client **
 ****************/

Net::Nic_client::Nic_client(Session_label const &label_arg,
                            Domain_name   const &domain_arg,
                            Allocator           &alloc,
                            Nic_client_dict     &new_nic_clients,
                            Configuration       &config)
:
	Nic_client_dict::Element { new_nic_clients, label_arg },
	_alloc                   { alloc },
	_config                  { config },
	_domain                  { domain_arg }
{ }


bool Nic_client::finish_construction(Env &env, Cached_timer &timer, Interface_list &interfaces,
                                     Nic_client_dict &old_nic_clients)
{
	char const *error = "";
	old_nic_clients.with_element(
		label(),
		[&] /* handle_match */ (Nic_client &old_nic_client)
		{
			/* reuse existing interface */
			Nic_client_interface &interface = *old_nic_client._crit->interface_ptr;
			old_nic_client._crit->interface_ptr = nullptr;
			interface.domain_name(domain());
			_crit.construct(&interface);
		},
		[&] /* handle_no_match */ ()
		{
			/* create a new interface */
			if (_config.verbose()) {
				log("[", domain(), "] create NIC client: ", label()); }

			try {
				_crit.construct(new (_alloc)
					Nic_client_interface(env, timer, _alloc, interfaces, _config, domain(), label()));
			}
			catch (Insufficient_ram_quota) { error = "NIC session RAM quota"; }
			catch (Insufficient_cap_quota) { error = "NIC session CAP quota"; }
			catch (Service_denied)         { error = "NIC session denied"; }
		}
	);
	if (_crit.constructed())
		return true;

	if (_config.verbose())
		log("[", domain(), "] invalid NIC client: ", label(), " (", error, ")");
	return false;
}


Net::Nic_client::~Nic_client()
{
	if (!_crit.constructed())
		return;

	/* if the interface was yet not reused by another NIC client, destroy it */
	if (_crit->interface_ptr) {
		if (_config.verbose()) {
			log("[", domain(), "] destroy NIC client: ", label()); }
		destroy(_alloc, _crit->interface_ptr);
	}
}


/*******************************
 ** Nic_client_interface_base **
 *******************************/

Net::Nic_client_interface_base::
     Nic_client_interface_base(Domain_name   const &domain_name,
                               Session_label const &label,
                               bool          const &session_link_state)
:
	_domain_name_ptr    { &domain_name },
	_label              { label },
	_session_link_state { session_link_state }
{ }


void Net::Nic_client_interface_base::handle_domain_ready_state(bool state)
{
	_domain_ready = state;
}


bool Net::Nic_client_interface_base::interface_link_state() const
{
	return _domain_ready && _session_link_state;
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
