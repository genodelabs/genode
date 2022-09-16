/*
 * \brief  IP routing entry
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
#include <ip_rule.h>
#include <domain.h>

using namespace Net;
using namespace Genode;


Ip_rule::Ip_rule(Domain_dict &domains, Xml_node const node)
:
	Direct_rule { node },
	_domain     { domains.deprecated_find_by_domain_attr<Invalid>(node) }
{ }
