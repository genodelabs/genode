/*
 * \brief  Test the DHCP functionality of the NIC router
 * \author Martin Stein
 * \date   2020-11-23
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <nic.h>
#include <ipv4_config.h>
#include <dhcp_client.h>

/* Genode includes */
#include <net/ethernet.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

using namespace Net;
using namespace Genode;


class Main : public Nic_handler,
             public Dhcp_client_handler
{
	private:

		Env                            &_env;
		Attached_rom_dataspace          _config_rom         { _env, "config" };
		Xml_node                        _config             { _config_rom.xml() };
		Timer::Connection               _timer              { _env };
		Heap                            _heap               { &_env.ram(), &_env.rm() };
		bool                     const  _verbose            { _config.attribute_value("verbose", false) };
		Net::Nic                        _nic                { _env, _heap, *this, _verbose };
		Constructible<Dhcp_client>      _dhcp_client        { };
		bool                            _link_state         { false };
		Reconstructible<Ipv4_config>    _ip_config          { };
		Timer::One_shot_timeout<Main>   _initial_delay      { _timer, *this, &Main::_handle_initial_delay };

		void _handle_initial_delay(Duration);

	public:

		struct Invalid_arguments : Exception { };

		Main(Env &env);


		/*****************
		 ** Nic_handler **
		 *****************/

		void handle_eth(Ethernet_frame &eth,
		                Size_guard     &size_guard) override;

		void handle_link_state(bool link_state) override
		{
			if (!_link_state && link_state) {
				_dhcp_client.construct(_heap, _timer, _nic, *this);
			}
			if (_link_state && !link_state && ip_config().valid) {
				ip_config(Ipv4_config { });
			}
			_link_state = link_state;
		};


		/*************************
		 ** Dhcp_client_handler **
		 *************************/

		void ip_config(Ipv4_config const &ip_config) override;

		Ipv4_config const &ip_config() const override { return *_ip_config; }
};


void Main::ip_config(Ipv4_config const &ip_config)
{
	if (_verbose) {
		log("IP config: ", ip_config); }

	_ip_config.construct(ip_config);
}


void Main::_handle_initial_delay(Duration)
{
	log("Initialized");
	_nic.handle_link_state();
}


Main::Main(Env &env) : _env(env)
{
	_initial_delay.schedule(Microseconds { 1000000 });
}


void Main::handle_eth(Ethernet_frame &eth,
                      Size_guard     &size_guard)
{
	try {
		/* print receipt message */
		if (_verbose) {
			log("rcv ", eth); }

		if (!ip_config().valid) {
			_dhcp_client->handle_eth(eth, size_guard); }
		else {
			throw Drop_packet_inform("IP config still valid");
		}
	}
	catch (Drop_packet_inform exception) {
		if (_verbose) {
			log("drop packet: ", exception.msg); }
	}
}


void Component::construct(Env &env) { static Main main(env); }
