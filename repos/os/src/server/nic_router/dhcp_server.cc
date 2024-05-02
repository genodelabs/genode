/*
 * \brief  DHCP server role of a domain
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <dhcp_server.h>
#include <interface.h>
#include <domain.h>
#include <configuration.h>
#include <xml_node.h>

using namespace Net;
using namespace Genode;


/**********************
 ** Dhcp_server_base **
 **********************/

Dhcp_server_base::Dhcp_server_base(Allocator &alloc) : _alloc { alloc } { }


bool Dhcp_server_base::finish_construction(Xml_node const &node, Domain const &domain)
{
	bool result = true;
	node.for_each_sub_node("dns-server", [&] (Xml_node const &sub_node) {
		if (!result)
			return;

		Dns_server::construct(
			_alloc, sub_node.attribute_value("ip", Ipv4_address { }),
			[&] /* handle_success */ (Dns_server &server)
			{
				_dns_servers.insert_as_tail(server);
			},
			[&] /* handle_failure */ ()
			{
				result = _invalid(domain, "invalid DNS server entry");
			}
		);
	});
	if (!result)
		return result;

	node.with_optional_sub_node("dns-domain", [&] (Xml_node const &sub_node) {
		xml_node_with_attribute(sub_node, "name", [&] (Xml_attribute const &attr) {
			_dns_domain_name.set_to(attr);

			if (domain.config().verbose() &&
			    !_dns_domain_name.valid()) {

				log("[", domain, "] rejecting oversized DNS "
				    "domain name from DHCP server configuration");
			}
		});
	});
	return result;
}


Dhcp_server_base::~Dhcp_server_base()
{
	_dns_servers.destroy_each(_alloc);
}


bool Dhcp_server_base::_invalid(Domain const &domain,
                                char   const *reason)
{
	if (domain.config().verbose()) {
		log("[", domain, "] invalid DHCP server (", reason, ")"); }

	return false;
}


/*****************
 ** Dhcp_server **
 *****************/

bool Dhcp_server::dns_servers_empty() const
{
	if (_dns_config_from_ptr)
		return _resolve_dns_config_from().dns_servers_empty();
	return _dns_servers.empty();
}


Dhcp_server::Dhcp_server(Xml_node const node, Allocator &alloc)
:
	Dhcp_server_base(alloc),
	_ip_lease_time  (_init_ip_lease_time(node)),
	_ip_first(node.attribute_value("ip_first", Ipv4_address())),
	_ip_last(node.attribute_value("ip_last", Ipv4_address())),
	_ip_first_raw(_ip_first.to_uint32_little_endian()),
	_ip_count(_ip_last.to_uint32_little_endian() - _ip_first_raw + 1),
	_ip_alloc(alloc, _ip_count)
{ }


bool Dhcp_server::finish_construction(Xml_node const node,
                                      Domain_dict &domains,
                                      Domain &domain,
                                      Ipv4_address_prefix const &interface)
{
	if (!Dhcp_server_base::finish_construction(node, domain))
		return false;

	if (_dns_servers.empty() && !_dns_domain_name.valid()) {
		Domain_name dns_config_from = node.attribute_value("dns_config_from", Domain_name());
		if (dns_config_from != Domain_name()) {
			bool result = true;
			domains.with_element(dns_config_from,
				[&] (Domain &remote_domain) { _dns_config_from_ptr = &remote_domain; },
				[&] { result = _invalid(domain, "invalid dns_config_from attribute"); });
			if (!result)
				return result;
		}
	}
	if (!interface.prefix_matches(_ip_first)) {
		return _invalid(domain, "first IP does not match domain subnet"); }

	if (!interface.prefix_matches(_ip_last)) {
		return _invalid(domain, "last IP does not match domain subnet"); }

	if (interface.address.is_in_range(_ip_first, _ip_last)) {
		return _invalid(domain, "IP range contains IP address of domain"); }

	return true;
}


Microseconds Dhcp_server::_init_ip_lease_time(Xml_node const node)
{
	uint64_t ip_lease_time_sec =
		node.attribute_value("ip_lease_time_sec", (uint64_t)0);

	if (!ip_lease_time_sec) {
		ip_lease_time_sec = DEFAULT_IP_LEASE_TIME_SEC;
	}
	return Microseconds((uint64_t)ip_lease_time_sec * 1000 * 1000);
}


void Dhcp_server::print(Output &output) const
{
	_dns_servers.for_each([&] (Dns_server const &dns_server) {
		Genode::print(output, "DNS server ", dns_server.ip(), ", ");
	});
	_dns_domain_name.with_string([&] (Dns_domain_name::String const &str) {
		Genode::print(output, "DNS domain name ", str, ", ");
	});
	with_dns_config_from([&] (Domain &domain) {
		Genode::print(output, "DNS config from ", domain, ", "); });

	Genode::print(output, "IP first ", _ip_first,
	                        ", last ", _ip_last,
	                       ", count ", _ip_count,
	                  ", lease time ", _ip_lease_time.value / 1000 / 1000, " sec");
}


bool Dhcp_server::config_equal_to_that_of(Dhcp_server const &other) const
{
	return _ip_lease_time.value == other._ip_lease_time.value &&
	       _dns_servers.equal_to(other._dns_servers)          &&
	       _dns_domain_name.equal_to(other._dns_domain_name);
}


Ipv4_config const &Dhcp_server::_resolve_dns_config_from() const
{
	return _dns_config_from_ptr->ip_config();
}


Dhcp_server::Alloc_ip_result Dhcp_server::alloc_ip()
{
	Alloc_ip_result result = Alloc_ip_error();
	_ip_alloc.alloc().with_result(
		[&] (addr_t ip_raw) { result = Alloc_ip_result(Ipv4_address::from_uint32_little_endian(ip_raw + _ip_first_raw)); },
		[&] (auto) { });
	return result;
}


bool Dhcp_server::alloc_ip(Ipv4_address const &ip)
{
	return _ip_alloc.alloc_addr(ip.to_uint32_little_endian() - _ip_first_raw);
}


void Dhcp_server::free_ip(Ipv4_address const &ip)
{
	ASSERT(_ip_alloc.free(ip.to_uint32_little_endian() - _ip_first_raw));
}


bool Dhcp_server::has_invalid_remote_dns_cfg() const
{
	if (_dns_config_from_ptr)
		return !_dns_config_from_ptr->ip_config().valid();
	return false;
}


/*********************
 ** Dhcp_allocation **
 *********************/

Dhcp_allocation::Dhcp_allocation(Interface      &interface,
                             Ipv4_address const &ip,
                             Mac_address  const &mac,
                             Cached_timer       &timer,
                             Microseconds        lifetime)
:
	_interface(interface), _ip(ip), _mac(mac),
	_timeout(timer, *this, &Dhcp_allocation::_handle_timeout)
{
	_interface.dhcp_stats().alive++;
	_timeout.schedule(lifetime);
}


Dhcp_allocation::~Dhcp_allocation()
{
	_interface.dhcp_stats().alive--;
	_interface.dhcp_stats().destroyed++;
}


void Dhcp_allocation::lifetime(Microseconds lifetime)
{
	_timeout.schedule(lifetime);
}


bool Dhcp_allocation::_higher(Mac_address const &mac) const
{
	return memcmp(mac.addr, _mac.addr, sizeof(_mac.addr)) > 0;
}


void Dhcp_allocation::print(Output &output) const
{
	Genode::print(output, "MAC ", _mac, " IP ", _ip);
}


void Dhcp_allocation::_handle_timeout(Duration)
{
	_interface.dhcp_allocation_expired(*this);
}
