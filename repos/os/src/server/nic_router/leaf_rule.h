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

#ifndef _LEAF_RULE_H_
#define _LEAF_RULE_H_

/* local includes */
#include <rule.h>

namespace Genode { class Xml_node; }

namespace Net {

	class Domain;
	class Domain_tree;
	class Leaf_rule;
}

class Net::Leaf_rule : public Rule
{
	protected:

		Domain &_domain;

		static Domain &_find_domain(Domain_tree            &domains,
		                            Genode::Xml_node const  node);

	public:

		Leaf_rule(Domain_tree &domains, Genode::Xml_node const node);


		/***************
		 ** Accessors **
		 ***************/

		Domain &domain() const { return _domain; }
};

#endif /* _LEAF_RULE_H_ */
