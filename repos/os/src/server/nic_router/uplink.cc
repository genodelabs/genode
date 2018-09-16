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

/* local includes */
#include <uplink.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/*****************
 ** Uplink_base **
 *****************/

Net::Uplink_base::Uplink_base(Xml_node const &node)
:
	_label  { node.attribute_value("label",  Session_label::String()) },
	_domain { node.attribute_value("domain", Domain_name()) }
{ }


/************
 ** Uplink **
 ************/

void Uplink::_invalid(char const *reason) const
{
	if (_config.verbose()) {
		log("[", domain(), "] invalid uplink: ", *this, " (", reason, ")"); }

	throw Invalid();
}


Net::Uplink::Uplink(Xml_node    const &node,
                    Allocator         &alloc,
                    Uplink_tree       &old_uplinks,
                    Env               &env,
                    Timer::Connection &timer,
                    Interface_list    &interfaces,
                    Configuration     &config)
:
	Uplink_base     { node },
	Avl_string_base { label().string() },
	_alloc          { alloc },
	_config         { config }
{
	/* if an interface with this label already exists, reuse it */
	try {
		Uplink &old_uplink = old_uplinks.find_by_name(label());
		Uplink_interface &interface = old_uplink._interface();
		old_uplink._interface = Pointer<Uplink_interface>();
		interface.domain_name(domain());
		_interface = interface;
	}
	/* if not, create a new one */
	catch (Uplink_tree::No_match) {
		if (config.verbose()) {
			log("[", domain(), "] request uplink NIC session: ", *this); }

		try {
			_interface = *new (_alloc)
				Uplink_interface { env, timer, alloc, interfaces, config,
				                   domain(), label() };
		}
		catch (Insufficient_ram_quota) { _invalid("NIC session RAM quota"); }
		catch (Insufficient_cap_quota) { _invalid("NIC session CAP quota"); }
		catch (Service_denied)         { _invalid("NIC session denied"); }
	}
}


Net::Uplink::~Uplink()
{
	/* if the interface was yet not reused by another uplink, destroy it */
	try {
		Uplink_interface &interface = _interface();
		if (_config.verbose()) {
			log("[", domain(), "] close uplink NIC session: ", *this); }

		destroy(_alloc, &interface);
	}
	catch (Pointer<Uplink_interface>::Invalid) { }
}


void Net::Uplink::print(Output &output) const
{
	if (label() == Session_label()) {
		Genode::print(output, "?"); }
	else {
		Genode::print(output, label()); }
}


/***************************
 ** Uplink_interface_base **
 ***************************/

Net::Uplink_interface_base::Uplink_interface_base(Domain_name   const &domain_name,
                                                  Session_label const &label)
:
	_domain_name { domain_name },
	_label       { label }
{ }


/**********************
 ** Uplink_interface **
 **********************/

Net::Uplink_interface::Uplink_interface(Env                 &env,
                                        Timer::Connection   &timer,
                                        Genode::Allocator   &alloc,
                                        Interface_list      &interfaces,
                                        Configuration       &config,
                                        Domain_name   const &domain_name,
                                        Session_label const &label)
:
	Uplink_interface_base { domain_name, label },
	Nic::Packet_allocator { &alloc },
	Nic::Connection       { env, this, BUF_SIZE, BUF_SIZE, label.string() },
	_link_state_handler   { env.ep(), *this,
	                        &Uplink_interface::_handle_link_state },
	_interface            { env.ep(), timer, mac_address(), alloc,
	                        Mac_address(), config, interfaces, *rx(), *tx(),
	                        _link_state, *this }
{
	/* install packet stream signal handlers */
	rx_channel()->sigh_ready_to_ack   (_interface.sink_ack());
	rx_channel()->sigh_packet_avail   (_interface.sink_submit());
	tx_channel()->sigh_ack_avail      (_interface.source_ack());
	tx_channel()->sigh_ready_to_submit(_interface.source_submit());

	/* initialize link state handling */
	Nic::Connection::link_state_sigh(_link_state_handler);
	_link_state = link_state();
}


void Net::Uplink_interface::_handle_link_state()
{
	_link_state = link_state();
	_interface.handle_link_state();
}
