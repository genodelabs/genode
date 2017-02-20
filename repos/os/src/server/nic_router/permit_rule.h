/*
 * \brief  Rules for permitting ports in the context of a transport rule
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PERMIT_RULE_H_
#define _PERMIT_RULE_H_

/* local includes */
#include <leaf_rule.h>

/* Genode includes */
#include <util/avl_tree.h>
#include <net/port.h>

namespace Genode { class Output; }

namespace Net {

	class Permit_rule;
	class Permit_any_rule;
	class Permit_single_rule;
	class Permit_single_rule_tree;
}


struct Net::Permit_rule : Leaf_rule
{
	Permit_rule(Domain_tree &domains, Genode::Xml_node const node);


	/*********
	 ** log **
	 *********/

	virtual void print(Genode::Output &output) const = 0;
};


struct Net::Permit_any_rule : Permit_rule
{
	Permit_any_rule(Domain_tree &domains, Genode::Xml_node const node);


	/*********
	 ** log **
	 *********/

	void print(Genode::Output &output) const;
};


class Net::Permit_single_rule : public Permit_rule,
                                public Genode::Avl_node<Permit_single_rule>
{
	private:

		Port const _port;

	public:

		Permit_single_rule(Domain_tree            &domains,
		                   Genode::Xml_node const  node);

		Permit_single_rule const &find_by_port(Port const port) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Permit_single_rule *rule);


		/***************
		 ** Accessors **
		 ***************/

		Port port() const { return _port; }
};


struct Net::Permit_single_rule_tree : Genode::Avl_tree<Permit_single_rule>
{
	struct No_match : Genode::Exception { };

	Permit_single_rule const &find_by_port(Port const port) const;
};

#endif /* _PERMIT_RULE_H_ */
