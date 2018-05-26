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


Pointer<Permit_any_rule>
Transport_rule::_read_permit_any_rule(Domain_tree    &domains,
                                      Xml_node const  node,
                                      Allocator      &alloc)
{
	try {
		Xml_node sub_node = node.sub_node("permit-any");
		return Pointer<Permit_any_rule>(*new (alloc)
		                                Permit_any_rule(domains, sub_node));
	}
	catch (Xml_node::Nonexistent_sub_node) { }
	return Pointer<Permit_any_rule>();
}


Transport_rule::Transport_rule(Domain_tree    &domains,
                               Xml_node const  node,
                               Allocator      &alloc,
                               Cstring  const &protocol,
                               Configuration  &config,
                               Domain   const &domain)
:
	Direct_rule(node),
	_alloc(alloc),
	_permit_any_rule(_read_permit_any_rule(domains, node, alloc))
{
	/* skip specific permit rules if all ports are permitted anyway */
	try {
		Permit_any_rule &permit_any_rule = _permit_any_rule();
		if (config.verbose()) {
			log("[", domain, "] ", protocol, " permit-any rule: ", permit_any_rule);
			log("[", domain, "] ", protocol, " rule: dst ", _dst);
		}
		return;
	} catch (Pointer<Permit_any_rule>::Invalid) { }

	/* read specific permit rules */
	node.for_each_sub_node("permit", [&] (Xml_node const node) {
		Permit_single_rule &rule = *new (alloc)
			Permit_single_rule(domains, node);

		_permit_single_rules.insert(&rule);
		if (config.verbose()) {
			log("[", domain, "] ", protocol, " permit rule: ", rule); }
	});
	/* drop the transport rule if it has no permitted ports */
	if (!_permit_single_rules.first()) {
		throw Invalid(); }

	if (config.verbose()) {
		log("[", domain, "] ", protocol, " rule: dst ", _dst); }
}


Transport_rule::~Transport_rule()
{
	_permit_single_rules.destroy_each(_alloc);
	try { destroy(_alloc, &_permit_any_rule()); }
	catch (Pointer<Permit_any_rule>::Invalid) { }
}


Permit_rule const &Transport_rule::permit_rule(Port const port) const
{
	try { return _permit_any_rule(); }
	catch (Pointer<Permit_any_rule>::Invalid) { }
	return _permit_single_rules.find_by_port(port);
}
