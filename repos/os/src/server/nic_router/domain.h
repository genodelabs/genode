/*
 * \brief  Reflects an effective domain configuration node
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DOMAIN_H_
#define _DOMAIN_H_

/* local includes */
#include <forward_rule.h>
#include <transport_rule.h>
#include <nat_rule.h>
#include <ip_rule.h>
#include <arp_cache.h>
#include <port_allocator.h>
#include <pointer.h>
#include <ipv4_config.h>
#include <dhcp_server.h>
#include <interface.h>
#include <dictionary.h>

/* Genode includes */
#include <util/reconstructible.h>

namespace Genode {

	class Xml_generator;
	class Allocator;
}

namespace Net {

	class Interface;
	class Configuration;
	class Domain;
	using Domain_name = Genode::String<160>;
	class Domain_dict;
	class Domain_link_stats;
	class Domain_object_stats;
}


struct Net::Domain_object_stats
{
	Genode::size_t destroyed { 0 };

	void dissolve_interface(Interface_object_stats const &stats);

	void report(Genode::Xml_generator &xml);
};


struct Net::Domain_link_stats : Domain_object_stats
{
	Genode::size_t refused_for_ram   { 0 };
	Genode::size_t refused_for_ports { 0 };
	Genode::size_t destroyed         { 0 };

	void dissolve_interface(Interface_link_stats const &stats);

	void report(Genode::Xml_generator &xml);
};


class Net::Domain_dict : public Dictionary<Domain, Domain_name>
{
	public:

		template <typename NO_MATCH_EXCEPTION>
		Domain &deprecated_find_by_name(Domain_name const &domain_name)
		{
			Domain *dom_ptr { nullptr };
			with_element(
				domain_name,
				[&] /* match_fn */ (Domain &dom) { dom_ptr = &dom; },
				[&] /* no_match_fn */ () { throw NO_MATCH_EXCEPTION { }; }
			);
			return *dom_ptr;
		}

		template <typename NO_MATCH_EXCEPTION>
		Domain &deprecated_find_by_domain_attr(Genode::Xml_node const &node)
		{
			Domain_name const domain_name {
				node.attribute_value("domain", Domain_name { }) };

			return deprecated_find_by_name<NO_MATCH_EXCEPTION>(domain_name);
		}
};


class Net::Domain : public List<Domain>::Element,
                    public Domain_dict::Element
{
	private:

		Configuration                        &_config;
		Genode::Xml_node                      _node;
		Genode::Allocator                    &_alloc;
		Ip_rule_list                          _ip_rules             { };
		Forward_rule_tree                     _tcp_forward_rules    { };
		Forward_rule_tree                     _udp_forward_rules    { };
		Transport_rule_list                   _tcp_rules            { };
		Transport_rule_list                   _udp_rules            { };
		Ip_rule_list                          _icmp_rules           { };
		Port_allocator                        _tcp_port_alloc       { };
		Port_allocator                        _udp_port_alloc       { };
		Port_allocator                        _icmp_port_alloc      { };
		Nat_rule_tree                         _nat_rules            { };
		Interface_list                        _interfaces           { };
		unsigned long                         _interface_cnt        { 0 };
		Pointer<Dhcp_server>                  _dhcp_server          { };
		Genode::Reconstructible<Ipv4_config>  _ip_config;
		bool                            const _ip_config_dynamic    { !ip_config().valid() };
		List<Domain>                          _ip_config_dependents { };
		Arp_cache                             _arp_cache            { *this };
		Arp_waiter_list                       _foreign_arp_waiters  { };
		Link_side_tree                        _tcp_links            { };
		Link_side_tree                        _udp_links            { };
		Link_side_tree                        _icmp_links           { };
		Genode::size_t                        _tx_bytes             { 0 };
		Genode::size_t                        _rx_bytes             { 0 };
		bool                            const _verbose_packets;
		bool                            const _verbose_packet_drop;
		bool                            const _trace_packets;
		bool                            const _icmp_echo_server;
		bool                            const _use_arp;
		Genode::Session_label           const _label;
		Domain_link_stats                     _udp_stats            { };
		Domain_link_stats                     _tcp_stats            { };
		Domain_link_stats                     _icmp_stats           { };
		Domain_object_stats                   _arp_stats            { };
		Domain_object_stats                   _dhcp_stats           { };
		unsigned long                         _dropped_fragm_ipv4   { 0 };

		void _read_forward_rules(Genode::Cstring  const &protocol,
		                         Domain_dict            &domains,
		                         Genode::Xml_node const  node,
		                         char             const *type,
		                         Forward_rule_tree      &rules);

		void _read_transport_rules(Genode::Cstring  const &protocol,
		                           Domain_dict            &domains,
		                           Genode::Xml_node const  node,
		                           char             const *type,
		                           Transport_rule_list    &rules);

		void _invalid(char const *reason) const;

		void _log_ip_config() const;

		void _prepare_reconstructing_ip_config();

		void _finish_reconstructing_ip_config();

		template <typename FUNC>
		void _reconstruct_ip_config(FUNC && functor)
		{
			_prepare_reconstructing_ip_config();
			functor(_ip_config);
			_finish_reconstructing_ip_config();
		}

		void __FIXME__dissolve_foreign_arp_waiters();

	public:

		struct Invalid          : Genode::Exception { };
		struct Ip_config_static : Genode::Exception { };
		struct No_next_hop      : Genode::Exception { };

		Domain(Configuration          &config,
		       Genode::Xml_node const &node,
		       Domain_name      const &name,
		       Genode::Allocator      &alloc,
		       Domain_dict            &domain_dict);

		~Domain();

		void init(Domain_dict &domains);

		void deinit();

		Ipv4_address const &next_hop(Ipv4_address const &ip) const;

		void ip_config_from_dhcp_ack(Dhcp_packet &dhcp_ack);

		void discard_ip_config();

		void try_reuse_ip_config(Domain const &domain);

		Link_side_tree &links(L3_protocol const protocol);

		void attach_interface(Interface &interface);

		void detach_interface(Interface &interface);

		void interface_updates_domain_object(Interface &interface);

		void raise_rx_bytes(Genode::size_t bytes) { _rx_bytes += bytes; }

		void raise_tx_bytes(Genode::size_t bytes) { _tx_bytes += bytes; }

		void report(Genode::Xml_generator &xml);

		void add_dropped_fragm_ipv4(unsigned long dropped_fragm_ipv4);

		bool ready() const;

		void update_ready_state();


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		bool                         verbose_packets()     const { return _verbose_packets; }
		bool                         verbose_packet_drop() const { return _verbose_packet_drop; }
		bool                         trace_packets()       const { return _trace_packets; }
		bool                         icmp_echo_server()    const { return _icmp_echo_server; }
		bool                         use_arp()             const { return _use_arp; }
		Genode::Session_label const &label()               const { return _label; }
		Ipv4_config           const &ip_config()           const { return *_ip_config; }
		List<Domain>                &ip_config_dependents()      { return _ip_config_dependents; }
		Domain_name           const &name()                const { return Domain_dict::Element::name; }
		Ip_rule_list                &ip_rules()                  { return _ip_rules; }
		Forward_rule_tree           &tcp_forward_rules()         { return _tcp_forward_rules; }
		Forward_rule_tree           &udp_forward_rules()         { return _udp_forward_rules; }
		Transport_rule_list         &tcp_rules()                 { return _tcp_rules; }
		Transport_rule_list         &udp_rules()                 { return _udp_rules; }
		Ip_rule_list                &icmp_rules()                { return _icmp_rules; }
		Nat_rule_tree               &nat_rules()                 { return _nat_rules; }
		Interface_list              &interfaces()                { return _interfaces; }
		Configuration               &config()              const { return _config; }
		Dhcp_server                 &dhcp_server();
		Arp_cache                   &arp_cache()                 { return _arp_cache; }
		Arp_waiter_list             &foreign_arp_waiters()       { return _foreign_arp_waiters; }
		Link_side_tree              &tcp_links()                 { return _tcp_links; }
		Link_side_tree              &udp_links()                 { return _udp_links; }
		Link_side_tree              &icmp_links()                { return _icmp_links; }
		Domain_link_stats           &udp_stats()                 { return _udp_stats; }
		Domain_link_stats           &tcp_stats()                 { return _tcp_stats; }
		Domain_link_stats           &icmp_stats()                { return _icmp_stats; }
		Domain_object_stats         &arp_stats()                 { return _arp_stats; }
		Domain_object_stats         &dhcp_stats()                { return _dhcp_stats; }
		bool                         ip_config_dynamic() const   { return _ip_config_dynamic; };
};

#endif /* _DOMAIN_H_ */
