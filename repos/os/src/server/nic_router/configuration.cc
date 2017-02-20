/*
 * \brief  Reflects the current router configuration through objects
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
#include <configuration.h>

/* Genode includes */
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

using namespace Net;
using namespace Genode;


/***************
 ** Utilities **
 ***************/

static unsigned read_rtt_sec(Xml_node const node)
{
	unsigned const rtt_sec = node.attribute_value("rtt_sec", 0UL);
	if (!rtt_sec) {
		warning("fall back to default rtt_sec=\"3\"");
		return 3;
	}
	return rtt_sec;
}


/*******************
 ** Configuration **
 *******************/

Configuration::Configuration(Xml_node const node, Allocator &alloc)
:
	_alloc(alloc), _verbose(node.attribute_value("verbose", false)),
	_rtt_sec(read_rtt_sec(node)), _node(node)
{
	/* read domains */
	node.for_each_sub_node("domain", [&] (Xml_node const node) {
		try { _domains.insert(*new (_alloc) Domain(*this, node, _alloc)); }
		catch (Domain::Invalid) { warning("invalid domain"); }
	});
	/* as they must resolve domain names, create rules after domains */
	_domains.for_each([&] (Domain &domain) {
		if (_verbose) {
			log("Domain: ", domain); }

		domain.create_rules(_domains);
	});
}
