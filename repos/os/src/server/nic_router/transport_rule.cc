/*
 * \brief  Rule for allowing direct TCP/UDP traffic between two interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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


Permit_any_rule *Transport_rule::_read_permit_any(Domain_tree    &domains,
                                                  Xml_node const &node,
                                                  Allocator      &alloc)
{
	try {
		Xml_node sub_node = node.sub_node("permit-any");
		return new (alloc) Permit_any_rule(domains, sub_node);
	}
	catch (Xml_node::Nonexistent_sub_node) { }
	catch (Rule::Invalid) { warning("invalid permit-any rule"); }
	return nullptr;
}


Transport_rule::Transport_rule(Domain_tree    &domains,
                               Xml_node const &node,
                               Allocator      &alloc,
                               Cstring  const &protocol,
                               Configuration  &config)
:
	Direct_rule(node), _permit_any(_read_permit_any(domains, node, alloc))
{
	/* skip specific permit rules if all ports are permitted anyway */
	if (_permit_any) {
		if (config.verbose()) {
			log("  ", protocol, " rule: ", _dst, " ", *_permit_any); }

		return;
	}

	/* read specific permit rules */
	node.for_each_sub_node("permit", [&] (Xml_node const &node) {
		try {
			Permit_single_rule &rule = *new (alloc)
				Permit_single_rule(domains, node);

			_permit_single_rules.insert(&rule);
			if (config.verbose()) {
				log(protocol, " rule: ", _dst, " ", rule); }
		}
		catch (Rule::Invalid) { warning("invalid permit rule"); }
	});
	/* drop the transport rule if it has no permitted ports */
	if (!_permit_single_rules.first()) {
		throw Invalid(); }
}


Permit_rule const &Transport_rule::permit_rule(uint16_t const port) const
{
	if (_permit_any) { return *_permit_any; }
	return _permit_single_rules.find_by_port(port);
}
