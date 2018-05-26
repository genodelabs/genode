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

#ifndef _IP_RULE_H_
#define _IP_RULE_H_

/* local includes */
#include <direct_rule.h>

namespace Net {

	class Domain;
	class Domain_tree;

	class  Ip_rule;
	struct Ip_rule_list : Direct_rule_list<Ip_rule> { };
}


class Net::Ip_rule : public Direct_rule<Ip_rule>
{
	private:

		Domain &_domain;

		static Domain &_find_domain(Domain_tree            &domains,
		                            Genode::Xml_node const  node);

	public:

		Ip_rule(Domain_tree &domains, Genode::Xml_node const node);


		/***************
		 ** Accessors **
		 ***************/

		Domain &domain() const { return _domain; }
};

#endif /* _IP_RULE_H_ */
