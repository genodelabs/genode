/*
 * \brief  Routing rule for direct traffic between two interfaces
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
#include <direct_rule.h>

using namespace Net;
using namespace Genode;


Direct_rule_base::Direct_rule_base(Xml_node const node)
:
	_dst(node.attribute_value("dst", Ipv4_address_prefix()))
{
	if (!_dst.valid()) {
		throw Invalid(); }
}


void Direct_rule_base::print(Output &output) const
{
	Genode::print(output, _dst);
}
