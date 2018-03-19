/*
 * \brief  Rule for forwarding a TCP/UDP port of the router to an interface
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FORWARD_RULE_H_
#define _FORWARD_RULE_H_

/* local includes */
#include <leaf_rule.h>
#include <avl_tree.h>

/* Genode includes */
#include <util/avl_tree.h>
#include <net/ipv4.h>
#include <net/port.h>

namespace Net {

	class Forward_rule;
	class Forward_rule_tree;
	class Forward_link;
	class Forward_link_tree;
}


class Net::Forward_rule : public Leaf_rule,
                          public Genode::Avl_node<Forward_rule>
{
	private:

		Port         const _port;
		Ipv4_address const _to;

	public:

		Forward_rule(Domain_tree &domains, Genode::Xml_node const node);

		Forward_rule const &find_by_port(Port const port) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Forward_rule *rule) {
			return rule->_port.value > _port.value; }


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address const &to() const { return _to; }
};


struct Net::Forward_rule_tree : Avl_tree<Forward_rule>
{
	struct No_match : Genode::Exception { };

	Forward_rule const &find_by_port(Port const port) const;
};

#endif /* _FORWARD_RULE_H_ */
