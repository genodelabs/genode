/*
 * \brief  IP routing entry
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IP_RULE_H_
#define _IP_RULE_H_

/* local includes */
#include <leaf_rule.h>
#include <direct_rule.h>

namespace Net {

	class  Ip_rule;
	struct Ip_rule_list : Direct_rule_list<Ip_rule> { };
}


struct Net::Ip_rule : Leaf_rule, Direct_rule<Ip_rule>
{
	public:

		Ip_rule(Domain_tree &domains, Genode::Xml_node const &node);
};

#endif /* _IP_RULE_H_ */
