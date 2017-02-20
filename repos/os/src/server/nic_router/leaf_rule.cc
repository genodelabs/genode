/*
 * \brief  Routing rule that defines a target interface
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
#include <leaf_rule.h>
#include <domain.h>

using namespace Net;
using namespace Genode;


Domain &Leaf_rule::_find_domain(Domain_tree    &domains,
                                Xml_node const  node)
{
	try {
		return domains.find_by_name(
			node.attribute_value("domain", Domain_name()));
	}
	catch (Domain_tree::No_match) { throw Invalid(); }
}


Leaf_rule::Leaf_rule(Domain_tree &domains, Xml_node const node)
:
	_domain(_find_domain(domains, node))
{ }
