/*
 * \brief  Rule for allowing direct TCP/UDP traffic between two interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

/* local includes */
#include <transport_rule.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


Transport_rule::Transport_rule(Ipv4_address_prefix const &dst,
                               Allocator                 &alloc)
:
	Direct_rule(dst),
	_alloc(alloc)
{ }


bool Transport_rule::finish_construction(Domain_dict    &domains,
                                         Xml_node const  node,
                                         Cstring  const &protocol,
                                         Configuration  &config,
                                         Domain   const &local_domain)
{
	/* try to find a permit-any rule first */
	bool error = false;
	node.with_optional_sub_node("permit-any", [&] (Xml_node const &permit_any_node) {
		domains.find_by_domain_attr(permit_any_node,
			[&] (Domain &remote_domain) { _permit_any_rule_ptr = new (_alloc) Permit_any_rule(remote_domain); },
			[&] { error = true; });
	});
	if (error)
		return false;

	/* skip specific permit rules if all ports are permitted anyway */
	if (_permit_any_rule_ptr) {
		if (config.verbose()) {
			log("[", local_domain, "] ", protocol, " permit-any rule: ", *_permit_any_rule_ptr);
			log("[", local_domain, "] ", protocol, " rule: dst ", _dst);
		}
		return true;
	}
	/* read specific permit rules */
	node.for_each_sub_node("permit", [&] (Xml_node const permit_node) {
		if (error)
			return;

		Port port = permit_node.attribute_value("port", Port(0));
		if (port == Port(0) || dynamic_port(port)) {
			error = true;
			return;
		}
		domains.find_by_domain_attr(permit_node,
			[&] (Domain &remote_domain) {
				Permit_single_rule &rule = *new (_alloc) Permit_single_rule(port, remote_domain);
				_permit_single_rules.insert(&rule);
				if (config.verbose())
					log("[", local_domain, "] ", protocol, " permit rule: ", rule); },
			[&] { error = true; });
	});
	return !error && (_permit_any_rule_ptr || _permit_single_rules.first());
}


Transport_rule::~Transport_rule()
{
	_permit_single_rules.destroy_each(_alloc);
	if (_permit_any_rule_ptr)
		destroy(_alloc, _permit_any_rule_ptr);
}
