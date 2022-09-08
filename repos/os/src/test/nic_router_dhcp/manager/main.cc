/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <util/list.h>
#include <os/reporter.h>

/* local includes */
#include <ipv4_address_prefix.h>

/* NIC router includes */
#include <dns.h>
#include <xml_node.h>

using namespace Net;
using namespace Genode;
using Domain_name = String<160>;

namespace Local { class Main; }


class Local::Main
{
	private:

		Env                    &_env;
		Heap                    _heap                   { &_env.ram(), &_env.rm() };
		Attached_rom_dataspace  _router_state_rom       { _env, "router_state" };
		Signal_handler<Main>    _router_state_handler   { _env.ep(), *this, &Main::_handle_router_state };
		Expanding_reporter      _router_config_reporter { _env, "config",  "router_config" };
		bool                    _router_config_outdated { true };
		Dns_server_list         _dns_servers            { };
		Dns_domain_name         _dns_domain_name        { _heap };

		void _handle_router_state();

	public:

		Main(Env &env);
};


Local::Main::Main(Env &env) : _env(env)
{
	log("Initialized");
	_router_state_rom.sigh(_router_state_handler);
	_handle_router_state();
}


void Local::Main::_handle_router_state()
{
	/* Request the moste recent content of the router state dataspace. */
	log("Read state of nic_router_2");
	_router_state_rom.update();

	/*
	 * Search for the uplink domain tag in the updated router state,
	 * read the new list of DNS servers from and compare it to the old list to
	 * see whether we have to re-configure the router.
	 */
	bool domain_found { false };
	_router_state_rom.xml().for_each_sub_node(
		"domain",
		[&] (Xml_node const &domain_node)
	{
		/*
		 * If we already found the uplink domain, refrain from inspecting
		 * further domain tags.
		 */
		if (domain_found) {
			return;
		}
		if (domain_node.attribute_value("name", Domain_name()) == "uplink") {

			domain_found = true;

			/*
			 * Consider re-configuring the router only when the uplink has
			 * a valid IPv4 config. This prevents us from propagating each,
			 * normally short-living "No-DNS-Server"-state that merely comes
			 * from the fact that the uplink has to redo DHCP and invalidates
			 * its Ipv4 config for this time.
			 */
			if (!domain_node.attribute_value("ipv4", Ipv4_address_prefix { }).valid()) {
				return;
			}

			/*
			 * Read out all DNS servers from the new uplink state
			 * and memorize them in a function-local list.
			 */
			Dns_server_list dns_servers { };
			domain_node.for_each_sub_node(
				"dns",
				[&] (Xml_node const &dns_node)
			{

				dns_servers.insert_as_tail(
					*new (_heap) Dns_server(dns_node.attribute_value("ip", Ipv4_address { })));
			});

			/*
			 * If the new list of DNS servers differs from the stored list,
			 * update the stored list, and remember to write out a new router
			 * configuration.
			 */
			if (!_dns_servers.equal_to(dns_servers)) {

				_dns_servers.destroy_each(_heap);
				dns_servers.for_each([&] (Dns_server const &dns_server) {
					_dns_servers.insert_as_tail(
						*new (_heap) Dns_server(dns_server.ip()));
				});
				_router_config_outdated = true;
			}
			dns_servers.destroy_each(_heap);

			/* read out new DNS domain name */
			Dns_domain_name dns_domain_name { _heap };
			domain_node.with_optional_sub_node("dns-domain", [&] (Xml_node const &sub_node) {
				xml_node_with_attribute(sub_node, "name", [&] (Xml_attribute const &attr) {
					dns_domain_name.set_to(attr);
				});
			});
			/* update stored DNS domain name if necessary */
			if (!_dns_domain_name.equal_to(dns_domain_name)) {
				_dns_domain_name.set_to(dns_domain_name);
				_router_config_outdated = true;
			}
		}
	});

	/*
	 * Write out a new router configuration with the updated list of
	 * DNS servers if necessary.
	 */
	if (_router_config_outdated) {

		log("Write config of nic_router_2");
		_router_config_reporter.generate([&] (Xml_generator &xml) {
			xml.node("report", [&] () {
				xml.attribute("bytes", "no");
				xml.attribute("stats", "no");
				xml.attribute("quota", "no");
				xml.attribute("config", "yes");
				xml.attribute("config_triggers", "yes");
				xml.attribute("interval_sec", "100");
			});
			xml.node("policy", [&] () {
				xml.attribute("label", "test_client -> ");
				xml.attribute("domain", "downlink");
			});
			xml.node("nic-client", [&] () {
				xml.attribute("domain", "uplink");
			});
			xml.node("domain", [&] () {
				xml.attribute("name", "uplink");
			});
			xml.node("domain", [&] () {
				xml.attribute("name", "downlink");
				xml.attribute("interface", "10.0.3.1/24");
				xml.node("dhcp-server", [&] () {
					xml.attribute("ip_first", "10.0.3.2");
					xml.attribute("ip_last", "10.0.3.2");
					_dns_servers.for_each([&] (Dns_server const &dns_server) {
						xml.node("dns-server", [&] () {
							xml.attribute("ip", String<16>(dns_server.ip()));
						});
					});
					_dns_domain_name.with_string(
						[&] (Dns_domain_name::String const &str)
					{
						xml.node("dns-domain", [&] () {
							xml.attribute("name", str);
						});
					});
				});
			});
		});
		_router_config_outdated = false;
	}
}


void Component::construct(Env &env) { static Local::Main main(env); }
